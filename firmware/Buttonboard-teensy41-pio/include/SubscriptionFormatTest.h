/*
 * NT4 Subscription Format Test
 * 
 * Since OutlineViewer can see the data but our subscriptions aren't working,
 * let's test different subscription formats to match what actually works.
 */

#ifndef SUBSCRIPTION_FORMAT_TEST_H
#define SUBSCRIPTION_FORMAT_TEST_H

#include <Arduino.h>
#include <NativeEthernet.h>

class SubscriptionFormatTest {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    int testPhase;
    unsigned long lastTestTime;
    int messagesSent;
    int messagesReceived;
    
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
        
        debugPrint("📤 Test " + String(testPhase) + " - Sending: " + message);
        
        size_t msgLen = message.length();
        
        // WebSocket frame with masking
        client.write(0x81); // Text frame
        client.write((uint8_t)(msgLen | 0x80)); // Length with mask bit
        
        // Masking key
        uint8_t maskKey[4] = {0x12, 0x34, 0x56, 0x78}; // Fixed for debugging
        client.write(maskKey, 4);
        
        // Masked payload
        for (size_t i = 0; i < msgLen; i++) {
            client.write((uint8_t)(message[i] ^ maskKey[i % 4]));
        }
        
        client.flush();
        messagesSent++;
    }
    
    void processIncomingData() {
        static uint8_t buffer[2048];
        static size_t bufferPos = 0;
        
        // Read all available data into buffer
        while (client.available() && bufferPos < sizeof(buffer) - 1) {
            buffer[bufferPos++] = client.read();
        }
        
        if (bufferPos > 0) {
            messagesReceived++;
            debugPrint("📥 RECEIVED DATA! (" + String(bufferPos) + " bytes) - Test " + String(testPhase) + " SUCCESS!");
            
            // Parse WebSocket frames
            size_t pos = 0;
            while (pos + 2 <= bufferPos) {
                uint8_t firstByte = buffer[pos++];
                uint8_t secondByte = buffer[pos++];
                
                uint8_t opcode = firstByte & 0x0F;
                uint8_t payloadLen = secondByte & 0x7F;
                
                if (opcode == 1 && payloadLen > 0 && pos + payloadLen <= bufferPos) { // Text frame
                    String payload = "";
                    for (int i = 0; i < payloadLen; i++) {
                        payload += (char)buffer[pos + i];
                    }
                    pos += payloadLen;
                    
                    debugPrint("📄 Payload: " + payload);
                    
                    // Look for our data
                    if (payload.indexOf("ControlBoard") >= 0) {
                        debugPrint("🎯 SUCCESS! Found ControlBoard data!");
                    }
                    if (payload.indexOf("SmartDashboard") >= 0) {
                        debugPrint("🎯 SUCCESS! Found SmartDashboard data!");
                    }
                    if (payload.indexOf("battery") >= 0) {
                        debugPrint("🔋 SUCCESS! Found battery data!");
                    }
                } else {
                    pos += payloadLen;
                }
            }
            
            // Reset buffer
            bufferPos = 0;
        }
    }
    
    void runTest(int phase) {
        String message = "";
        
        switch (phase) {
            case 1:
                debugPrint("🧪 TEST 1: Subscribe to exact ControlBoard/battery/voltage");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/battery/voltage\"],\"subuid\":1}}]";
                break;
                
            case 2:
                debugPrint("🧪 TEST 2: Subscribe to ControlBoard/ prefix");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/\"],\"subuid\":2}}]";
                break;
                
            case 3:
                debugPrint("🧪 TEST 3: Subscribe to ControlBoard prefix (no slash)");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard\"],\"subuid\":3}}]";
                break;
                
            case 4:
                debugPrint("🧪 TEST 4: Subscribe with options");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/\"],\"subuid\":4,\"options\":{\"periodic\":0.1,\"all\":false}}}]";
                break;
                
            case 5:
                debugPrint("🧪 TEST 5: Subscribe to everything");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/\"],\"subuid\":5}}]";
                break;
                
            case 6:
                debugPrint("🧪 TEST 6: Subscribe with all=true");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/\"],\"subuid\":6,\"options\":{\"all\":true}}}]";
                break;
                
            case 7:
                debugPrint("🧪 TEST 7: Multiple topics in one subscription");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/battery/voltage\",\"SmartDashboard/BatteryVoltage\"],\"subuid\":7}}]";
                break;
                
            case 8:
                debugPrint("🧪 TEST 8: Empty topic list (should get everything)");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[],\"subuid\":8}}]";
                break;
                
            case 9:
                debugPrint("🧪 TEST 9: Wildcard subscription");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"*\"],\"subuid\":9}}]";
                break;
                
            case 10:
                debugPrint("🧪 TEST 10: No subuid (see if that's the problem)");
                message = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/\"]}}]";
                break;
                
            default:
                debugPrint("✅ All tests complete!");
                return;
        }
        
        sendMaskedMessage(message);
    }
    
public:
    SubscriptionFormatTest(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        testPhase(0),
        lastTestTime(0),
        messagesSent(0),
        messagesReceived(0)
    {
    }
    
    bool connect() {
        debugPrint("=== NT4 SUBSCRIPTION FORMAT TESTER ===");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                isConnected = true;
                connectionStartTime = millis();
                testPhase = 0;
                lastTestTime = 0;
                debugPrint("🧪 Will start tests in 3 seconds...");
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
        
        unsigned long now = millis();
        unsigned long uptime = now - connectionStartTime;
        
        // Process any incoming data
        processIncomingData();
        
        // Run tests every 5 seconds after initial delay
        if (uptime > 3000 && (now - lastTestTime >= 5000)) {
            testPhase++;
            if (testPhase <= 10) {
                runTest(testPhase);
                lastTestTime = now;
            } else if (testPhase == 11) {
                debugPrint("🏁 TEST SUMMARY:");
                debugPrint("   Messages sent: " + String(messagesSent));
                debugPrint("   Messages received: " + String(messagesReceived));
                if (messagesReceived == 0) {
                    debugPrint("   ❌ NO RESPONSES - subscription format issue");
                    debugPrint("   💡 Try checking OutlineViewer's connection method");
                } else {
                    debugPrint("   ✅ Got responses! Check which test worked.");
                }
                testPhase = 12; // Stop testing
            }
        }
    }
    
    bool connected() const { return isConnected; }
};

#endif // SUBSCRIPTION_FORMAT_TEST_H