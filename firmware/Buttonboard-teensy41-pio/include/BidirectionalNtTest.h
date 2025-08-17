/*
 * Bidirectional NT4 Test
 * 
 * Theory: Robot only sends data to clients that have BOTH published and subscribed
 * This matches what we see in OutlineViewer - Dashboard@1 shows both pub/sub
 */

#ifndef BIDIRECTIONAL_NT_TEST_H
#define BIDIRECTIONAL_NT_TEST_H

#include <Arduino.h>
#include <NativeEthernet.h>

class BidirectionalNTTest {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    int phase;
    unsigned long lastPhaseTime;
    int messagesSent;
    int messagesReceived;
    bool hasPublished;
    bool hasSubscribed;
    
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
        
        debugPrint("📤 Sending: " + message);
        
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
    }
    
    void processIncomingData() {
        static uint8_t buffer[2048];
        static size_t bufferPos = 0;
        
        // Read all available data
        while (client.available() && bufferPos < sizeof(buffer) - 1) {
            buffer[bufferPos++] = client.read();
        }
        
        if (bufferPos > 0) {
            messagesReceived++;
            debugPrint("📥 RECEIVED " + String(bufferPos) + " bytes! Phase " + String(phase));
            
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
                    
                    debugPrint("📄 Message: " + payload);
                    
                    // Check what we received
                    if (payload.indexOf("announce") >= 0) {
                        debugPrint("📢 Got announce message");
                    }
                    if (payload.indexOf("ControlBoard") >= 0) {
                        debugPrint("🎯 SUCCESS! Got ControlBoard data!");
                    }
                    if (payload.indexOf("battery") >= 0) {
                        debugPrint("🔋 SUCCESS! Got battery data!");
                    }
                    if (payload.indexOf("11.75") >= 0) {
                        debugPrint("⚡ SUCCESS! Got our hardcoded voltage!");
                    }
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0; // Reset buffer
        }
    }
    
    void runPhase(int phaseNum) {
        switch (phaseNum) {
            case 1:
                debugPrint("🏁 PHASE 1: Publish a value to establish ourselves as a publisher");
                sendMaskedMessage("[{\"method\":\"announce\",\"params\":{\"name\":\"/SmartDashboard/TeensyAlive\",\"type\":\"boolean\",\"pubuid\":1}}]");
                break;
                
            case 2:
                debugPrint("🏁 PHASE 2: Send the actual value");
                sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyAlive\":true}}]");
                hasPublished = true;
                break;
                
            case 3:
                debugPrint("🏁 PHASE 3: Now subscribe to ControlBoard data (with leading slash)");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":1}}]");
                hasSubscribed = true;
                break;
                
            case 4:
                debugPrint("🏁 PHASE 4: Subscribe to SmartDashboard data too");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/\"],\"subuid\":2}}]");
                break;
                
            case 5:
                debugPrint("🏁 PHASE 5: Subscribe to specific battery topic");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":3}}]");
                break;
                
            case 6:
                debugPrint("🏁 PHASE 6: Try subscribe with options");
                sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":4,\"options\":{\"periodic\":0.1,\"all\":true}}}]");
                break;
                
            case 7:
                debugPrint("🏁 PHASE 7: Publish another value to keep us 'active'");
                sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyHeartbeat\":" + String(millis()) + "}}]");
                break;
                
            case 8:
                debugPrint("🏁 PHASE 8: Wait and monitor for incoming data...");
                debugPrint("Status: Published=" + String(hasPublished) + ", Subscribed=" + String(hasSubscribed));
                debugPrint("If we don't get data now, there's a deeper protocol issue.");
                break;
                
            default:
                if (phaseNum <= 15) {
                    debugPrint("🔄 PHASE " + String(phaseNum) + ": Waiting for data... (sent " + 
                              String(messagesSent) + ", received " + String(messagesReceived) + ")");
                    
                    // Send periodic heartbeat to stay "active"
                    if (phaseNum % 3 == 0) {
                        sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyHeartbeat\":" + String(millis()) + "}}]");
                    }
                } else {
                    debugPrint("🏁 TEST COMPLETE");
                    debugPrint("Final stats: Sent=" + String(messagesSent) + ", Received=" + String(messagesReceived));
                    if (messagesReceived == 0) {
                        debugPrint("❌ FAILED: Still no data received");
                        debugPrint("💡 Next step: Check if robot is actually running NT4 server");
                    } else {
                        debugPrint("✅ SUCCESS: Data received!");
                    }
                }
                break;
        }
    }
    
public:
    BidirectionalNTTest(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        phase(0),
        lastPhaseTime(0),
        messagesSent(0),
        messagesReceived(0),
        hasPublished(false),
        hasSubscribed(false)
    {
    }
    
    bool connect() {
        debugPrint("=== BIDIRECTIONAL NT4 TEST ===");
        debugPrint("Theory: Robot only sends data to clients that publish AND subscribe");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                isConnected = true;
                connectionStartTime = millis();
                phase = 0;
                lastPhaseTime = 0;
                debugPrint("🚀 Starting test phases in 2 seconds...");
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
        
        // Always process incoming data
        processIncomingData();
        
        // Run phases every 3 seconds after initial delay
        if (uptime > 2000 && (now - lastPhaseTime >= 3000)) {
            phase++;
            runPhase(phase);
            lastPhaseTime = now;
        }
    }
    
    bool connected() const { return isConnected; }
    
    void printStatus() {
        debugPrint("Status: Phase=" + String(phase) + ", Sent=" + String(messagesSent) + 
                  ", Received=" + String(messagesReceived));
        debugPrint("Flags: Published=" + String(hasPublished) + ", Subscribed=" + String(hasSubscribed));
    }
};

#endif // BIDIRECTIONAL_NT_TEST_H