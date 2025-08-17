/*
 * NT Traffic Monitor
 * 
 * Subscribe to EVERYTHING and show all NetworkTables activity
 * to see if the robot is sending ANY data at all.
 */

#ifndef NT_TRAFFIC_MONITOR_H
#define NT_TRAFFIC_MONITOR_H

#include <Arduino.h>
#include <NativeEthernet.h>

class NTTrafficMonitor {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    int messagesReceived;
    int announceCount;
    int valueCount;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] MONITOR: " + message);
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
                    
                    messagesReceived++;
                    analyzeMessage(payload);
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
    void analyzeMessage(const String& message) {
        debugPrint("📥 #" + String(messagesReceived) + ": " + message.substring(0, 200) + 
                  (message.length() > 200 ? "..." : ""));
        
        // Count message types
        if (message.indexOf("\"announce\"") >= 0) {
            announceCount++;
            debugPrint("📢 ANNOUNCE #" + String(announceCount));
            
            // Extract topic name if possible
            int nameStart = message.indexOf("\"name\":\"") + 8;
            int nameEnd = message.indexOf("\"", nameStart);
            if (nameStart > 7 && nameEnd > nameStart) {
                String topicName = message.substring(nameStart, nameEnd);
                debugPrint("   Topic: " + topicName);
            }
        }
        
        if (message.indexOf("\"setValues\"") >= 0) {
            valueCount++;
            debugPrint("🎉 *** VALUE MESSAGE #" + String(valueCount) + " ***");
            
            // Look for battery data
            if (message.indexOf("battery") >= 0 || message.indexOf("Battery") >= 0) {
                debugPrint("🔋 *** CONTAINS BATTERY DATA! ***");
            }
            if (message.indexOf("11.75") >= 0) {
                debugPrint("⚡ *** CONTAINS 11.75! ***");
            }
        }
        
        if (message.indexOf("\"update\"") >= 0) {
            valueCount++;
            debugPrint("🎉 *** UPDATE MESSAGE #" + String(valueCount) + " ***");
        }
        
        // Look for test data
        if (message.indexOf("TestValue") >= 0) {
            debugPrint("🧪 *** FOUND TEST VALUE! ***");
        }
        if (message.indexOf("timestamp") >= 0) {
            debugPrint("⏰ *** FOUND TIMESTAMP! ***");
        }
    }
    
public:
    NTTrafficMonitor(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        messagesReceived(0),
        announceCount(0),
        valueCount(0)
    {
    }
    
    bool connect() {
        debugPrint("=== NT TRAFFIC MONITOR ===");
        debugPrint("Goal: Monitor ALL NetworkTables activity");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                isConnected = true;
                connectionStartTime = millis();
                
                // Subscribe to EVERYTHING with maximum coverage
                delay(2000);
                debugPrint("📡 Subscribing to ALL topics...");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/\"],\"subuid\":1,\"options\":{\"all\":true}}}]");
                
                delay(1000);
                debugPrint("📡 Subscribing to SmartDashboard...");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/\"],\"subuid\":2,\"options\":{\"all\":true}}}]");
                
                delay(1000);
                debugPrint("📡 Subscribing to ControlBoard...");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":3,\"options\":{\"all\":true}}}]");
                
                debugPrint("🔍 Monitoring started - will show ALL robot activity for 2 minutes");
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
                printFinalSummary();
                isConnected = false;
            }
            return;
        }
        
        processIncomingData();
        
        unsigned long now = millis();
        unsigned long uptime = now - connectionStartTime;
        
        // Print summary every 30 seconds
        static unsigned long lastSummary = 0;
        if (now - lastSummary >= 30000) {
            debugPrint("📊 " + String(uptime / 1000) + "s: " + String(messagesReceived) + 
                      " total, " + String(announceCount) + " announces, " + String(valueCount) + " values");
            lastSummary = now;
        }
        
        // Stop after 2 minutes
        if (uptime > 120000) {
            debugPrint("⏰ 2 minute monitoring complete");
            printFinalSummary();
            client.stop();
            isConnected = false;
        }
    }
    
    bool connected() const { return isConnected; }
    
    void printFinalSummary() {
        debugPrint("=== TRAFFIC MONITOR SUMMARY ===");
        debugPrint("Total messages: " + String(messagesReceived));
        debugPrint("Announce messages: " + String(announceCount));
        debugPrint("Value messages: " + String(valueCount));
        
        if (messagesReceived == 0) {
            debugPrint("❌ NO TRAFFIC: Robot not sending any NT data");
        } else if (valueCount == 0) {
            debugPrint("❌ NO VALUES: Robot announces topics but never sends values");
            debugPrint("💡 Check if Robot.java is actually running the publish loop");
        } else {
            debugPrint("✅ ROBOT IS ACTIVE: Sending " + String(valueCount) + " value updates");
        }
        debugPrint("===============================");
    }
};

#endif // NT_TRAFFIC_MONITOR_H