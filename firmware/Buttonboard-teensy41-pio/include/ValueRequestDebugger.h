/*
 * NT4 Value Request Debugger
 * 
 * The robot is announcing topics but not sending values.
 * Let's try different methods to request the actual data.
 */

#ifndef VALUE_REQUEST_DEBUG_H
#define VALUE_REQUEST_DEBUG_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>

class ValueRequestDebugger {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    int nextSubId;
    int nextPubId;
    int requestAttempt;
    unsigned long lastRequestTime;
    bool gotAnnounce;
    int topicId;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] NT: " + message);
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
        
        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) {
            maskKey[i] = random(256);
        }
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
                    topicId = params["id"];
                    String type = params["type"];
                    
                    debugPrint("📢 ANNOUNCE: " + name + " (ID=" + String(topicId) + ", type=" + type + ")");
                    
                    if (name == "/ControlBoard/battery/voltage") {
                        debugPrint("🎯 Found our target topic! ID=" + String(topicId));
                        gotAnnounce = true;
                        requestAttempt = 0;
                        lastRequestTime = 0; // Trigger immediate request
                    }
                    
                } else if (method == "setValues" || method == "update") {
                    debugPrint("📊 VALUE MESSAGE!");
                    JsonObject params = messageObj["params"];
                    
                    for (JsonPair kv : params) {
                        String topic = kv.key().c_str();
                        JsonVariant value = kv.value();
                        debugPrint("🔋 " + topic + " = " + value.as<String>());
                        
                        if (topic == "/ControlBoard/battery/voltage") {
                            debugPrint("🎉 SUCCESS! Got battery voltage: " + value.as<String>() + "V");
                        }
                    }
                    
                } else if (method == "unannounce") {
                    String name = messageObj["params"]["name"];
                    debugPrint("📢 UNANNOUNCE: " + name);
                    
                } else {
                    debugPrint("❓ Unknown method: " + method);
                }
            }
        }
    }
    
    void tryValueRequest(int attempt) {
        String message = "";
        
        switch (attempt) {
            case 1:
                debugPrint("🧪 ATTEMPT 1: Subscribe with immediate=true");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"immediate\":true}}}]";
                break;
                
            case 2:
                debugPrint("🧪 ATTEMPT 2: Subscribe with all=true");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"all\":true}}}]";
                break;
                
            case 3:
                debugPrint("🧪 ATTEMPT 3: Subscribe with periodic=0.01 (very fast)");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"periodic\":0.01}}}]";
                break;
                
            case 4:
                debugPrint("🧪 ATTEMPT 4: Subscribe to broader /ControlBoard/ with immediate");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"immediate\":true,\"all\":true}}}]";
                break;
                
            case 5:
                debugPrint("🧪 ATTEMPT 5: Try getValue method (if it exists)");
                message = "[{\"method\":\"getValue\",\"params\":{\"name\":\"/ControlBoard/battery/voltage\"}}]";
                break;
                
            case 6:
                debugPrint("🧪 ATTEMPT 6: Request by topic ID");
                message = "[{\"method\":\"subscribe\",\"params\":{\"ids\":[" + String(topicId) + "],\"subuid\":" + 
                         String(nextSubId++) + "}}]";
                break;
                
            case 7:
                debugPrint("🧪 ATTEMPT 7: Subscribe with no options at all");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + "}}]";
                break;
                
            case 8:
                debugPrint("🧪 ATTEMPT 8: Unsubscribe and resubscribe");
                sendMaskedMessage("[{\"method\":\"unsubscribe\",\"params\":{\"subuid\":1}}]");
                delay(500);
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + "}}]";
                break;
                
            case 9:
                debugPrint("🧪 ATTEMPT 9: Check if we need to subscribe to SmartDashboard version");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/BatteryVoltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"immediate\":true}}}]";
                break;
                
            case 10:
                debugPrint("🧪 ATTEMPT 10: Try without leading slash");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"immediate\":true}}}]";
                break;
                
            default:
                debugPrint("❌ All attempts failed. Robot may not be publishing this data actively.");
                debugPrint("💡 Check Robot.java - make sure publishDataToControlBoard() is actually running every 100ms");
                return;
        }
        
        sendMaskedMessage(message);
    }
    
public:
    ValueRequestDebugger(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        nextSubId(1),
        nextPubId(1),
        requestAttempt(0),
        lastRequestTime(0),
        gotAnnounce(false),
        topicId(-1)
    {
    }
    
    bool connect() {
        debugPrint("=== NT4 VALUE REQUEST DEBUGGER ===");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                isConnected = true;
                connectionStartTime = millis();
                
                // Establish bidirectional connection
                delay(500);
                sendMaskedMessage("[{\"method\":\"announce\",\"params\":{\"name\":\"/SmartDashboard/TeensyDebugger\",\"type\":\"boolean\",\"pubuid\":" + String(nextPubId++) + "}}]");
                delay(200);
                sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyDebugger\":true}}]");
                delay(200);
                
                // Subscribe to get the announce message
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
                
                debugPrint("🔍 Waiting for announce message...");
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
        
        // Once we get the announce, try different value request methods
        if (gotAnnounce && (now - lastRequestTime >= 3000)) { // Every 3 seconds
            requestAttempt++;
            if (requestAttempt <= 10) {
                debugPrint("🔄 Request attempt " + String(requestAttempt) + "/10");
                tryValueRequest(requestAttempt);
                lastRequestTime = now;
            }
        }
    }
    
    bool connected() const { return isConnected; }
};

#endif // VALUE_REQUEST_DEBUG_H