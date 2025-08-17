/*
 * Safe Value Requester
 * 
 * We know "immediate:true" causes disconnection. Let's try only safe options
 * and focus on getting the robot to actually SEND the values.
 */

#ifndef SAFE_VALUE_REQUESTER_H
#define SAFE_VALUE_REQUESTER_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>

class SafeValueRequester {
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
    bool gotAnnounces;
    bool gotValues;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] SAFE: " + message);
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
                    
                    debugPrint("📢 ANNOUNCE: " + name + " (ID=" + String(id) + ")");
                    gotAnnounces = true;
                    
                } else if (method == "setValues") {
                    debugPrint("🎉 *** GOT SETVALUES! ***");
                    gotValues = true;
                    JsonObject params = messageObj["params"];
                    for (JsonPair kv : params) {
                        String topic = kv.key().c_str();
                        JsonVariant value = kv.value();
                        debugPrint("📊 " + topic + " = " + value.as<String>());
                        
                        if (topic.indexOf("battery") >= 0 || topic.indexOf("Battery") >= 0) {
                            debugPrint("🔋 *** BATTERY VALUE: " + value.as<String>() + "V ***");
                        }
                    }
                    
                } else if (method == "update") {
                    debugPrint("🎉 *** GOT UPDATE! ***");
                    gotValues = true;
                    JsonObject params = messageObj["params"];
                    for (JsonPair kv : params) {
                        String topic = kv.key().c_str();
                        JsonVariant value = kv.value();
                        debugPrint("📈 " + topic + " = " + value.as<String>());
                        
                        if (topic.indexOf("battery") >= 0 || topic.indexOf("Battery") >= 0) {
                            debugPrint("🔋 *** BATTERY VALUE: " + value.as<String>() + "V ***");
                        }
                    }
                }
            }
        }
    }
    
    void trySafeValueRequest(int attempt) {
        String message = "";
        
        switch (attempt) {
            case 1:
                debugPrint("🧪 SAFE ATTEMPT 1: Subscribe with all=true (no immediate)");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"all\":true}}}]";
                break;
                
            case 2:
                debugPrint("🧪 SAFE ATTEMPT 2: Subscribe with periodic=0.1 only");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"periodic\":0.1}}}]";
                break;
                
            case 3:
                debugPrint("🧪 SAFE ATTEMPT 3: Subscribe to SmartDashboard version");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/BatteryVoltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"all\":true}}}]";
                break;
                
            case 4:
                debugPrint("🧪 SAFE ATTEMPT 4: Publish something first, then subscribe");
                sendMaskedMessage("[{\"method\":\"announce\",\"params\":{\"name\":\"/SmartDashboard/TeensyRequest\",\"type\":\"boolean\",\"pubuid\":" + String(nextPubId++) + ",\"properties\":{}}}]");
                delay(500);
                sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyRequest\":true}}]");
                delay(500);
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"all\":true}}}]";
                break;
                
            case 5:
                debugPrint("🧪 SAFE ATTEMPT 5: Subscribe to broader /ControlBoard/ prefix");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"all\":true}}}]";
                break;
                
            case 6:
                debugPrint("🧪 SAFE ATTEMPT 6: Subscribe with no options at all");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + 
                         String(nextSubId++) + "}}]";
                break;
                
            case 7:
                debugPrint("🧪 SAFE ATTEMPT 7: Subscribe to everything /");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/\"],\"subuid\":" + 
                         String(nextSubId++) + ",\"options\":{\"all\":true}}}]";
                break;
                
            default:
                debugPrint("❌ All safe attempts completed");
                debugPrint("💡 Robot announces topics but doesn't send values");
                debugPrint("💡 Check if Robot.java publishDataToControlBoard() is actually running every 100ms");
                return;
        }
        
        sendMaskedMessage(message);
    }
    
public:
    SafeValueRequester(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        nextSubId(1),
        nextPubId(1),
        requestAttempt(0),
        lastRequestTime(0),
        gotAnnounces(false),
        gotValues(false)
    {
    }
    
    bool connect() {
        debugPrint("=== SAFE VALUE REQUESTER ===");
        debugPrint("Goal: Get values without disconnecting");
        
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
                unsigned long uptime = millis() - connectionStartTime;
                debugPrint("💔 Connection lost after " + String(uptime) + "ms");
                debugPrint("Last attempt that caused disconnection: " + String(requestAttempt));
                isConnected = false;
            }
            return;
        }
        
        processIncomingData();
        
        unsigned long now = millis();
        
        // Once we have announces, start trying safe value request methods
        if (gotAnnounces && !gotValues && (now - lastRequestTime >= 5000)) { // Every 5 seconds
            requestAttempt++;
            if (requestAttempt <= 7) {
                debugPrint("🔄 Safe value request attempt " + String(requestAttempt) + "/7");
                trySafeValueRequest(requestAttempt);
                lastRequestTime = now;
            }
        }
        
        // Print status every 10 seconds
        static unsigned long lastStatus = 0;
        if (now - lastStatus >= 10000) {
            debugPrint("📊 Status: Announces=" + String(gotAnnounces ? "YES" : "NO") + 
                      ", Values=" + String(gotValues ? "YES" : "NO") + 
                      ", Uptime=" + String((now - connectionStartTime) / 1000) + "s");
            lastStatus = now;
        }
    }
    
    bool connected() const { return isConnected; }
    bool hasValues() const { return gotValues; }
    
    void printSummary() {
        debugPrint("=== SAFE REQUESTER SUMMARY ===");
        debugPrint("Got announces: " + String(gotAnnounces ? "YES" : "NO"));
        debugPrint("Got values: " + String(gotValues ? "YES" : "NO"));
        debugPrint("Completed attempts: " + String(requestAttempt));
        
        if (gotValues) {
            debugPrint("🎉 SUCCESS! Found working value request method");
        } else {
            debugPrint("❌ NO VALUES: Robot may not be publishing data actively");
        }
        debugPrint("===============================");
    }
};

#endif // SAFE_VALUE_REQUESTER_H