/*
 * Careful NT4 Subscription Test
 * 
 * Since we know the connection is stable, let's carefully test subscription
 * messages one at a time to see which format works.
 */

#ifndef CAREFUL_SUBSCRIPTION_TEST_H
#define CAREFUL_SUBSCRIPTION_TEST_H

#include <Arduino.h>
#include <NativeEthernet.h>

class CarefulSubscriptionTest {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    bool handshakeComplete;
    int testPhase;
    unsigned long lastTestTime;
    int messagesReceived;
    int messagesSent;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] CAREFUL: " + message);
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
        
        if (response.indexOf("101") >= 0) {
            handshakeComplete = true;
            return true;
        }
        return false;
    }
    
    void sendMaskedMessage(const String& message) {
        if (!isConnected || !client.connected() || !handshakeComplete) {
            debugPrint("❌ Cannot send - not ready");
            return;
        }
        
        size_t msgLen = message.length();
        
        // WebSocket frame with masking
        client.write(0x81); // Text frame
        client.write((uint8_t)(msgLen | 0x80)); // Length with mask bit
        
        // Simple masking key
        uint8_t maskKey[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        client.write(maskKey, 4);
        
        // Masked payload
        for (size_t i = 0; i < msgLen; i++) {
            client.write((uint8_t)(message[i] ^ maskKey[i % 4]));
        }
        
        client.flush();
        messagesSent++;
        
        debugPrint("📤 Sent (#" + String(messagesSent) + "): " + message);
    }
    
    void processIncomingData() {
        static uint8_t buffer[2048];
        static size_t bufferPos = 0;
        
        while (client.available() && bufferPos < sizeof(buffer) - 1) {
            buffer[bufferPos++] = client.read();
        }
        
        if (bufferPos > 0) {
            messagesReceived++;
            debugPrint("📥 Received " + String(bufferPos) + " bytes (#" + String(messagesReceived) + ")");
            
            // Parse WebSocket frames
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
                    
                    debugPrint("📄 Payload: " + payload.substring(0, 200) + (payload.length() > 200 ? "..." : ""));
                    
                    // Look for success indicators
                    if (payload.indexOf("announce") >= 0) {
                        debugPrint("🎯 SUCCESS: Got announce message!");
                    }
                    if (payload.indexOf("ControlBoard") >= 0) {
                        debugPrint("🎯 SUCCESS: Found ControlBoard data!");
                    }
                    if (payload.indexOf("battery") >= 0) {
                        debugPrint("🔋 SUCCESS: Found battery data!");
                    }
                    if (payload.indexOf("11.75") >= 0) {
                        debugPrint("⚡ SUCCESS: Found voltage value!");
                    }
                    
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
    void runTestPhase(int phase) {
        String message = "";
        
        switch (phase) {
            case 1:
                debugPrint("🧪 PHASE 1: Simple subscription to everything");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/\"],\"subuid\":1}}]";
                break;
                
            case 2:
                debugPrint("🧪 PHASE 2: Subscribe to SmartDashboard");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/\"],\"subuid\":2}}]";
                break;
                
            case 3:
                debugPrint("🧪 PHASE 3: Subscribe to ControlBoard");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":3}}]";
                break;
                
            case 4:
                debugPrint("🧪 PHASE 4: Subscribe to specific battery topic");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":4}}]";
                break;
                
            case 5:
                debugPrint("🧪 PHASE 5: Subscribe with options");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":5,\"options\":{\"immediate\":true}}}]";
                break;
                
            default:
                debugPrint("🏁 All phases complete");
                return;
        }
        
        sendMaskedMessage(message);
    }
    
public:
    CarefulSubscriptionTest(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        handshakeComplete(false),
        testPhase(0),
        lastTestTime(0),
        messagesReceived(0),
        messagesSent(0)
    {
    }
    
    bool connect() {
        debugPrint("=== CAREFUL SUBSCRIPTION TEST ===");
        debugPrint("Goal: Test subscription messages one at a time");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                isConnected = true;
                connectionStartTime = millis();
                debugPrint("⏱️  Waiting 5 seconds before starting tests...");
                return true;
            } else {
                client.stop();
            }
        }
        
        return false;
    }
    
    void update() {
        if (!isConnected) return;
        
        if (!client.connected()) {
            unsigned long uptime = millis() - connectionStartTime;
            debugPrint("💔 Connection lost after " + String(uptime) + "ms");
            debugPrint("📊 Phase " + String(testPhase) + " caused disconnection");
            debugPrint("📈 Stats: Sent=" + String(messagesSent) + ", Received=" + String(messagesReceived));
            isConnected = false;
            return;
        }
        
        // Process any incoming data
        processIncomingData();
        
        unsigned long now = millis();
        unsigned long uptime = now - connectionStartTime;
        
        // Start tests after 5 seconds
        if (uptime > 5000 && testPhase == 0) {
            debugPrint("🚀 Starting subscription tests...");
            testPhase = 1;
            lastTestTime = now;
        }
        
        // Run test phases every 10 seconds
        if (testPhase > 0 && testPhase <= 5 && (now - lastTestTime >= 10000)) {
            debugPrint("🔍 Running test phase " + String(testPhase) + "...");
            runTestPhase(testPhase);
            testPhase++;
            lastTestTime = now;
        }
        
        // Final summary after all tests
        if (testPhase > 5 && (now - lastTestTime >= 10000)) {
            debugPrint("🏁 TEST COMPLETE");
            debugPrint("📊 Final stats: Sent=" + String(messagesSent) + ", Received=" + String(messagesReceived));
            debugPrint("✅ Connection stayed up for " + String(uptime / 1000) + " seconds");
            
            if (messagesReceived > 0) {
                debugPrint("🎉 SUCCESS: Got responses from robot!");
            } else {
                debugPrint("❌ NO RESPONSES: Check subscription format");
            }
            
            testPhase = 6; // Stop testing
        }
    }
    
    bool connected() const { return isConnected; }
    
    void printSummary() {
        debugPrint("=== CAREFUL TEST SUMMARY ===");
        debugPrint("Messages sent: " + String(messagesSent));
        debugPrint("Messages received: " + String(messagesReceived));
        debugPrint("Connection stable: " + String(isConnected ? "YES" : "NO"));
        
        if (messagesReceived > 0) {
            debugPrint("✅ Robot responded - subscription format works!");
        } else {
            debugPrint("❌ No responses - need to adjust format");
        }
        debugPrint("=============================");
    }
};

#endif // CAREFUL_SUBSCRIPTION_TEST_H