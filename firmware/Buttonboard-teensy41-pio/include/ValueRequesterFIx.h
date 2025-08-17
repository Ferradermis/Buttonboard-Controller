/*
 * NT4 Value Requester Fix
 * 
 * We're getting announces but not values. Let's try different methods
 * to actually get the current values from the robot.
 */

#ifndef VALUE_REQUESTER_FIX_H
#define VALUE_REQUESTER_FIX_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>

class ValueRequesterFix {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    int nextSubId;
    int requestAttempt;
    unsigned long lastRequestTime;
    bool gotAnnounces;
    
    // Store topic IDs from announces
    struct TopicInfo {
        int id;
        String name;
        String type;
    };
    TopicInfo knownTopics[10];
    int topicCount;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] FIX: " + message);
    }
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    bool performWebSocketHandshake() {
        String request = "GET /nt/ws HTTP/1.1\r\n";
        request += "Host: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                   String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort) + "\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: dGVlbnN5LW50LWNsaWVudA==\r\n";
        request += "Sec-WebSocket-Version: 13\r\n";
        request += "Sec-WebSocket-Protocol: networktables.first.wpi.edu\r\n";
        request += "\r\n";
        
        client.print(request);
        client.flush();
        
        unsigned long startTime = millis();
        String response = "";
        
        while (millis() - startTime < 3000) {
            if (client.available()) {
                response += (char)client.read();
                if (response.endsWith("\r\n\r\n")) break;
            }
            delay(1);
        }
        
        return response.indexOf("101") >= 0;
    }
    
    void sendMaskedMessage(const String& message) {
        if (!isConnected || !client.connected()) return;
        
        size_t msgLen = message.length();
        
        client.write(0x81);
        client.write((uint8_t)(msgLen | 0x80));
        
        uint8_t maskKey[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        client.write(maskKey, 4);
        
        for (size_t i = 0; i < msgLen; i++) {
            client.write((uint8_t)(message[i] ^ maskKey[i % 4]));
        }
        
        client.flush();
        debugPrint("📤 Sent: " + message);
    }
    
    void processIncomingData() {
        static uint8_t buffer[2048];
        static size_t bufferPos = 0;
        
        while (client.available() && bufferPos < sizeof(buffer) - 1) {
            buffer[bufferPos++] = client.read();
        }
        
        if (bufferPos > 0) {
            size_t pos = 0;
            while (pos + 2 <= bufferPos) {
                uint8_t firstByte = buffer[pos++];
                uint8_t secondByte = buffer[pos++];
                
                uint8_t opcode = firstByte & 0x0F;
                uint8_t payloadLen = secondByte & 0x7F;
                
                if (opcode == 1 && payloadLen > 0 && pos + payloadLen <= bufferPos) {
                    String payload = "";
                    for (int i = 0; i < payloadLen; i++) {
                        payload += (char)buffer[pos + i];
                    }
                    pos += payloadLen;
                    
                    debugPrint("📥 Received: " + payload);
                    parseMessage(payload);
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
    void parseMessage(const String& message) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            debugPrint("❌ JSON error: " + String(error.c_str()));
            return;
        }
        
        if (doc.is<JsonArray>()) {
            JsonArray messageArray = doc.as<JsonArray>();
            
            for (JsonObject messageObj : messageArray) {
                String method = messageObj["method"];
                
                if (method == "announce") {
                    JsonObject params = messageObj["params"];
                    String name = params["name"];
                    int id = params["id"];
                    String type = params["type"];
                    
                    debugPrint("📢 ANNOUNCE: " + name + " (ID=" + String(id) + ", type=" + type + ")");
                    
                    // Store topic info
                    if (topicCount < 10) {
                        knownTopics[topicCount].id = id;
                        knownTopics[topicCount].name = name;
                        knownTopics[topicCount].type = type;
                        topicCount++;
                    }
                    
                    gotAnnounces = true;
                    
                } else if (method == "setValues") {
                    debugPrint("🎉 GOT SETVALUES!");
                    JsonObject params = messageObj["params"];
                    for (JsonPair kv : params) {
                        String topic = kv.key().c_str();
                        JsonVariant value = kv.value();
                        debugPrint("📊 " + topic + " = " + value.as<String>());
                        
                        if (topic.indexOf("battery") >= 0 || topic.indexOf("Battery") >= 0) {
                            debugPrint("🔋 *** BATTERY VALUE: " + value.as<String>() + " ***");
                        }
                    }
                    
                } else if (method == "update") {
                    debugPrint("🎉 GOT UPDATE!");
                    JsonObject params = messageObj["params"];
                    for (JsonPair kv : params) {
                        String topic = kv.key().c_str();
                        JsonVariant value = kv.value();
                        debugPrint("📈 " + topic + " = " + value.as<String>());
                        
                        if (topic.indexOf("battery") >= 0 || topic.indexOf("Battery") >= 0) {
                            debugPrint("🔋 *** BATTERY VALUE: " + value.as<String>() + " ***");
                        }
                    }
                }
            }
        }
    }
    
    void tryValueRequest(int attempt) {
        String message = "";
        
        switch (attempt) {
            case 1:
                debugPrint("🧪 ATTEMPT 1: Subscribe with all=true");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"all\":true}}}]";
                break;
                
            case 2:
                debugPrint("🧪 ATTEMPT 2: Subscribe with immediate=true and all=true");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"immediate\":true,\"all\":true}}}]";
                break;
                
            case 3:
                debugPrint("🧪 ATTEMPT 3: Subscribe with periodic=0.1");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"periodic\":0.1}}}]";
                break;
                
            case 4:
                debugPrint("🧪 ATTEMPT 4: Subscribe by topic ID instead of name");
                if (topicCount > 0) {
                    message = "[{\"method\":\"subscribe\",\"params\":{\"ids\":[" + String(knownTopics[0].id) + "],\"subuid\":" + 
                             String(nextSubId++) + "}}]";
                } else {
                    debugPrint("No topic IDs available");
                    return;
                }
                break;
                
            case 5:
                debugPrint("🧪 ATTEMPT 5: Try getValue method");
                message = "[{\"method\":\"getValue\",\"params\":{\"name\":\"/ControlBoard/battery/voltage\"}}]";
                break;
                
            case 6:
                debugPrint("🧪 ATTEMPT 6: Subscribe to broader prefix with options");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"immediate\":true,\"all\":true,\"periodic\":0.1}}}]";
                break;
                
            case 7:
                debugPrint("🧪 ATTEMPT 7: Try SmartDashboard version");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/BatteryVoltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"immediate\":true,\"all\":true}}}]";
                break;
                
            case 8:
                debugPrint("🧪 ATTEMPT 8: Force request by sending setValues first");
                message = "[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyRequest\":true}}]";
                break;
                
            default:
                debugPrint("❌ All attempts failed. Robot may not be actively publishing values.");
                debugPrint("💡 Check if Robot.java publishDataToControlBoard() is actually running");
                return;
        }
        
        sendMaskedMessage(message);
    }
    
public:
    ValueRequesterFix(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        nextSubId(1),
        requestAttempt(0),
        lastRequestTime(0),
        gotAnnounces(false),
        topicCount(0)
    {
    }
    
    bool connect() {
        debugPrint("=== VALUE REQUESTER FIX ===");
        debugPrint("Goal: Get actual values, not just announces");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                isConnected = true;
                connectionStartTime = millis();
                
                // Start with basic subscription to get announces
                delay(2000);
                debugPrint("📡 Initial subscription to get announces...");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + String(nextSubId++) + "}}]");
                
                return true;
            } else {
                client.stop();
            }
        }
        
        return false;
    }
    
    void update() {
        if (!isConnected || !client.connected()) {
            if (isConnected) {
                debugPrint("💔 Connection lost");
                isConnected = false;
            }
            return;
        }
        
        processIncomingData();
        
        unsigned long now = millis();
        
        // Once we have announces, start trying different value request methods
        if (gotAnnounces && (now - lastRequestTime >= 5000)) { // Every 5 seconds
            requestAttempt++;
            if (requestAttempt <= 8) {
                debugPrint("🔄 Value request attempt " + String(requestAttempt) + "/8");
                tryValueRequest(requestAttempt);
                lastRequestTime = now;
            }
        }
    }
    
    bool connected() const { return isConnected; }
    
    void printSummary() {
        debugPrint("=== FIX SUMMARY ===");
        debugPrint("Got announces: " + String(gotAnnounces ? "YES" : "NO"));
        debugPrint("Known topics: " + String(topicCount));
        for (int i = 0; i < topicCount; i++) {
            debugPrint("  " + knownTopics[i].name + " (ID=" + String(knownTopics[i].id) + ")");
        }
        debugPrint("===================");
    }
};

#endif // VALUE_REQUESTER_FIX_H