/*
 * Minimal NT4.1 Test
 * 
 * Since sending messages seems to cause disconnection, let's try just
 * connecting and listening without sending anything initially.
 */

#ifndef MINIMAL_NT41_TEST_H
#define MINIMAL_NT41_TEST_H

#include <Arduino.h>
#include <NativeEthernet.h>

class MinimalNT41Test {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    bool handshakeComplete;
    int messagesReceived;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] MIN41: " + message);
    }
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    bool performWebSocketHandshake() {
        debugPrint("🤝 Starting minimal WebSocket handshake...");
        
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
            debugPrint("✅ WebSocket handshake successful");
            handshakeComplete = true;
            return true;
        } else {
            debugPrint("❌ WebSocket handshake failed");
            debugPrint("Response: " + response.substring(0, 100));
            return false;
        }
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
                    
                    debugPrint("📄 Message: " + payload.substring(0, 100) + (payload.length() > 100 ? "..." : ""));
                    
                    // Look for battery data
                    if (payload.indexOf("ControlBoard") >= 0) {
                        debugPrint("🎯 FOUND ControlBoard data!");
                    }
                    if (payload.indexOf("battery") >= 0) {
                        debugPrint("🔋 FOUND battery data!");
                    }
                    if (payload.indexOf("11.75") >= 0) {
                        debugPrint("⚡ FOUND our voltage value!");
                    }
                    
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
public:
    MinimalNT41Test(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        handshakeComplete(false),
        messagesReceived(0)
    {
    }
    
    bool connect() {
        debugPrint("=== MINIMAL NT4.1 TEST ===");
        debugPrint("Goal: Just connect and listen without sending messages");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                isConnected = true;
                connectionStartTime = millis();
                debugPrint("🎉 Connected! Will just listen for 60 seconds...");
                debugPrint("💡 If this stays connected, the issue is with our messages");
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
            debugPrint("📊 Received " + String(messagesReceived) + " messages total");
            
            if (uptime < 5000) {
                debugPrint("❌ Quick disconnect - robot doesn't like our connection");
            } else if (messagesReceived == 0) {
                debugPrint("❌ No data received - might need to send subscription first");
            } else {
                debugPrint("✅ Got data! Connection method works");
            }
            
            isConnected = false;
            return;
        }
        
        // Just listen for data
        processIncomingData();
        
        // Print status every 10 seconds
        static unsigned long lastStatus = 0;
        unsigned long now = millis();
        if (now - lastStatus >= 10000) {
            unsigned long uptime = now - connectionStartTime;
            debugPrint("📈 Status: " + String(uptime / 1000) + "s connected, " + 
                      String(messagesReceived) + " messages received");
            lastStatus = now;
            
            // After 30 seconds, if no data, the robot might expect us to send something
            if (uptime > 30000 && messagesReceived == 0) {
                debugPrint("💡 No data after 30s - robot might expect subscription messages");
            }
        }
        
        // Disconnect after 60 seconds
        if (now - connectionStartTime > 60000) {
            debugPrint("⏰ 60 second test complete");
            debugPrint("Final result: " + String(messagesReceived) + " messages received");
            client.stop();
            isConnected = false;
        }
    }
    
    bool connected() const { return isConnected; }
    
    void printSummary() {
        debugPrint("=== TEST SUMMARY ===");
        if (messagesReceived > 0) {
            debugPrint("✅ SUCCESS: Received " + String(messagesReceived) + " messages");
            debugPrint("💡 Robot sends data - our messaging is the problem");
        } else {
            debugPrint("❌ NO DATA: Robot didn't send anything");
            debugPrint("💡 Either need subscription, or wrong protocol");
        }
        debugPrint("====================");
    }
};

#endif // MINIMAL_NT41_TEST_H