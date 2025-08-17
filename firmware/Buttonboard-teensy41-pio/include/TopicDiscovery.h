/*
 * NT4 Topic Discovery
 * 
 * Since we're not seeing the announce for /ControlBoard/battery/voltage,
 * let's see ALL topics the robot is actually announcing.
 */

#ifndef TOPIC_DISCOVERY_H
#define TOPIC_DISCOVERY_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>

class TopicDiscovery {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    int nextSubId;
    int nextPubId;
    int topicsFound;
    bool hasRequestedAll;
    
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
        debugPrint("📤 " + message.substring(0, 80) + (message.length() > 80 ? "..." : ""));
    }
    
    void processIncomingData() {
        static uint8_t buffer[4096]; // Larger buffer for more data
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
                    
                    debugPrint("📥 " + payload);
                    parseMessage(payload);
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
    void parseMessage(const String& message) {
        DynamicJsonDocument doc(2048); // Larger for more data
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
                    topicsFound++;
                    JsonObject params = messageObj["params"];
                    String name = params["name"];
                    int id = params["id"];
                    String type = params["type"];
                    
                    debugPrint("🔍 TOPIC #" + String(topicsFound) + ": " + name + " (ID=" + String(id) + ", type=" + type + ")");
                    
                    // Look for battery-related topics
                    if (name.indexOf("battery") >= 0 || name.indexOf("Battery") >= 0) {
                        debugPrint("🔋 *** BATTERY TOPIC FOUND: " + name + " ***");
                    }
                    
                    // Look for ControlBoard topics
                    if (name.indexOf("ControlBoard") >= 0) {
                        debugPrint("🎯 *** CONTROLBOARD TOPIC: " + name + " ***");
                    }
                    
                    // Look for SmartDashboard topics
                    if (name.indexOf("SmartDashboard") >= 0) {
                        debugPrint("📊 *** SMARTDASHBOARD TOPIC: " + name + " ***");
                    }
                    
                } else if (method == "setValues" || method == "update") {
                    debugPrint("📈 VALUE UPDATE!");
                    JsonObject params = messageObj["params"];
                    
                    for (JsonPair kv : params) {
                        String topic = kv.key().c_str();
                        JsonVariant value = kv.value();
                        debugPrint("📊 " + topic + " = " + value.as<String>());
                        
                        if (topic.indexOf("battery") >= 0 || topic.indexOf("Battery") >= 0) {
                            debugPrint("🔋 *** GOT BATTERY VALUE: " + topic + " = " + value.as<String>() + " ***");
                        }
                    }
                    
                } else if (method == "unannounce") {
                    String name = messageObj["params"]["name"];
                    debugPrint("❌ REMOVED: " + name);
                    
                } else {
                    debugPrint("❓ Method: " + method);
                }
            }
        }
    }
    
public:
    TopicDiscovery(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        nextSubId(1),
        nextPubId(1),
        topicsFound(0),
        hasRequestedAll(false)
    {
    }
    
    bool connect() {
        debugPrint("=== NT4 TOPIC DISCOVERY ===");
        debugPrint("Will find ALL topics the robot is publishing");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                isConnected = true;
                connectionStartTime = millis();
                
                // Establish bidirectional connection
                delay(500);
                sendMaskedMessage("[{\"method\":\"announce\",\"params\":{\"name\":\"/SmartDashboard/TeensyDiscovery\",\"type\":\"boolean\",\"pubuid\":" + String(nextPubId++) + "}}]");
                delay(200);
                sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyDiscovery\":true}}]");
                delay(200);
                
                debugPrint("🔍 Requesting ALL topics...");
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
        
        unsigned long uptime = millis() - connectionStartTime;
        
        // After 2 seconds, request to see ALL topics
        if (!hasRequestedAll && uptime > 2000) {
            debugPrint("📡 Subscribing to EVERYTHING to see all topics...");
            
            // Try different ways to get all topics
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/\"],\"subuid\":" + String(nextSubId++) + "}}]");
            delay(500);
            
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"*\"],\"subuid\":" + String(nextSubId++) + "}}]");
            delay(500);
            
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[],\"subuid\":" + String(nextSubId++) + "}}]");
            delay(500);
            
            // Try specific prefixes
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
            delay(500);
            
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
            delay(500);
            
            // Try without leading slash
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
            delay(500);
            
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
            
            hasRequestedAll = true;
            debugPrint("📡 All subscription requests sent!");
        }
        
        // Print summary every 10 seconds
        static unsigned long lastSummary = 0;
        if (uptime > 10000 && millis() - lastSummary >= 10000) {
            debugPrint("📋 SUMMARY: Found " + String(topicsFound) + " topics so far");
            lastSummary = millis();
            
            if (topicsFound == 0) {
                debugPrint("⚠️  NO TOPICS FOUND - this suggests a subscription format issue");
            }
        }
    }
    
    bool connected() const { return isConnected; }
    
    void printSummary() {
        debugPrint("=== DISCOVERY COMPLETE ===");
        debugPrint("Total topics found: " + String(topicsFound));
        if (topicsFound == 0) {
            debugPrint("❌ No topics discovered - check robot code and subscription format");
        }
    }
};

#endif // TOPIC_DISCOVERY_H