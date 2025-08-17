/*
 * MINIMAL NT4 Connection Test
 * 
 * This version focuses on establishing a stable connection first
 * and only sends messages after confirming the connection is stable.
 */

#ifndef MINIMAL_NT_CLIENT_H
#define MINIMAL_NT_CLIENT_H

#include <Arduino.h>
#include <NativeEthernet.h>

class MinimalNTClient {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    unsigned long lastStatusPrint;
    bool hasTriedSubscription;
    
    void debugPrint(const String& message) {
        Serial.println("NT: " + message);
    }
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    bool performWebSocketHandshake() {
        debugPrint("🤝 Starting minimal WebSocket handshake...");
        
        // Send the simplest possible WebSocket upgrade request
        client.print("GET /nt/ws HTTP/1.1\r\n");
        client.print("Host: ");
        client.print(robotIP[0]); client.print(".");
        client.print(robotIP[1]); client.print(".");
        client.print(robotIP[2]); client.print(".");
        client.print(robotIP[3]); client.print(":");
        client.print(ntPort); client.print("\r\n");
        client.print("Upgrade: websocket\r\n");
        client.print("Connection: Upgrade\r\n");
        client.print("Sec-WebSocket-Key: dGVlbnN5LW50LWNsaWVudA==\r\n");
        client.print("Sec-WebSocket-Version: 13\r\n");
        client.print("Sec-WebSocket-Protocol: networktables.first.wpi.edu\r\n");
        client.print("\r\n");
        client.flush();
        
        // Wait for response
        unsigned long startTime = millis();
        String response = "";
        
        while (millis() - startTime < 3000) {
            if (client.available()) {
                response += (char)client.read();
                if (response.endsWith("\r\n\r\n")) break;
            }
            delay(1);
        }
        
        if (response.indexOf("101") >= 0) {
            debugPrint("✅ WebSocket handshake successful");
            return true;
        } else {
            debugPrint("❌ Handshake failed. Response: " + response.substring(0, 100));
            return false;
        }
    }
    
    void sendMaskedMessage(const String& message) {
        if (!isConnected || !client.connected()) return;
        
        size_t msgLen = message.length();
        
        // Frame header: 0x81 = text frame, final fragment
        client.write(0x81);
        
        // Payload length with mask bit (0x80)
        client.write((uint8_t)(msgLen | 0x80));
        
        // Generate masking key
        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) {
            maskKey[i] = random(256);
        }
        client.write(maskKey, 4);
        
        // Send masked payload
        for (size_t i = 0; i < msgLen; i++) {
            uint8_t maskedByte = message[i] ^ maskKey[i % 4];
            client.write(maskedByte);
        }
        
        client.flush();
        debugPrint("📤 Sent masked message (" + String(msgLen) + " bytes)");
    }
    
    void processIncomingData() {
        static String buffer = "";
        static bool inFrame = false;
        static uint8_t expectedLen = 0;
        
        while (client.available()) {
            uint8_t byte = client.read();
            
            if (!inFrame) {
                if (byte == 0x81) { // Text frame
                    inFrame = true;
                    buffer = "";
                    expectedLen = 0;
                }
            } else if (expectedLen == 0) {
                expectedLen = byte & 0x7F; // Remove mask bit
                if (expectedLen == 0) inFrame = false;
            } else {
                buffer += (char)byte;
                if (buffer.length() >= expectedLen) {
                    debugPrint("📥 Received: " + buffer.substring(0, 50) + 
                              (buffer.length() > 50 ? "..." : ""));
                    
                    // Reset for next frame
                    inFrame = false;
                    expectedLen = 0;
                    buffer = "";
                }
            }
        }
    }
    
public:
    MinimalNTClient(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        lastStatusPrint(0),
        hasTriedSubscription(false)
    {
    }
    
    bool connect() {
        debugPrint("=== MINIMAL NT4 CONNECTION TEST ===");
        debugPrint("Connecting to " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                  String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort));
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connection established");
            
            if (performWebSocketHandshake()) {
                isConnected = true;
                connectionStartTime = millis();
                hasTriedSubscription = false;
                debugPrint("🎉 Connection ready! Will wait before sending messages...");
                return true;
            } else {
                client.stop();
            }
        } else {
            debugPrint("❌ TCP connection failed");
        }
        
        return false;
    }
    
    void update() {
        if (!isConnected) return;
        
        unsigned long now = millis();
        unsigned long uptime = now - connectionStartTime;
        
        // Check if connection is still alive
        if (!client.connected()) {
            debugPrint("💔 Connection lost after " + String(uptime) + "ms");
            isConnected = false;
            return;
        }
        
        // Process any incoming data
        processIncomingData();
        
        // Print status every 5 seconds
        if (now - lastStatusPrint >= 5000) {
            debugPrint("💓 Connection alive for " + String(uptime) + "ms");
            lastStatusPrint = now;
            
            // After 10 seconds of stable connection, try sending subscription
            if (!hasTriedSubscription && uptime > 10000) {
                debugPrint("📡 Connection stable for 10s - trying multiple subscriptions...");
                
                // Try subscribing to specific keys first
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/BatteryVoltage\"],\"subuid\":1}}]");
                delay(500);
                
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/TestValue\"],\"subuid\":2}}]");
                delay(500);
                
                // Try the broader subscription
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/\"],\"subuid\":3}}]");
                delay(500);
                
                // Try ControlBoard table
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/\"],\"subuid\":4}}]");
                
                debugPrint("📡 Sent 4 different subscription requests");
                hasTriedSubscription = true;
            }
        }
    }
    
    bool connected() const { return isConnected; }
    
    unsigned long getUptime() const {
        if (!isConnected) return 0;
        return millis() - connectionStartTime;
    }
    
    void testConnectionStability() {
        debugPrint("=== CONNECTION STABILITY TEST ===");
        debugPrint("Will hold connection for 60 seconds without sending data...");
        
        unsigned long testStart = millis();
        while (millis() - testStart < 60000) { // 60 seconds
            if (!client.connected()) {
                unsigned long survived = millis() - testStart;
                debugPrint("❌ Connection failed after " + String(survived) + "ms");
                debugPrint("This suggests a timeout or protocol issue");
                return;
            }
            
            // Just process incoming data, don't send anything
            processIncomingData();
            
            // Print status every 10 seconds
            if ((millis() - testStart) % 10000 < 100) {
                debugPrint("⏱️  " + String((millis() - testStart) / 1000) + "s - connection stable");
            }
            
            delay(100);
        }
        
        if (client.connected()) {
            debugPrint("🎉 SUCCESS! Connection survived 60 seconds");
            debugPrint("Protocol is working - disconnection issue is likely message-related");
        }
    }
};

#endif // MINIMAL_NT_CLIENT_H