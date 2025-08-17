/*
 * NT4 Protocol Debugger
 * This version logs EVERYTHING to help us understand what's happening
 */

#ifndef NT_PROTOCOL_DEBUGGER_H
#define NT_PROTOCOL_DEBUGGER_H

#include <Arduino.h>
#include <NativeEthernet.h>

class NT4ProtocolDebugger {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    unsigned long lastStatusPrint;
    bool hasTriedSubscription;
    int messagesSent;
    int messagesReceived;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] NT: " + message);
    }
    
    void hexDump(const uint8_t* data, size_t length, const String& label) {
        debugPrint("=== HEX DUMP: " + label + " (" + String(length) + " bytes) ===");
        for (size_t i = 0; i < length; i += 16) {
            String line = String(i, HEX) + ": ";
            for (size_t j = 0; j < 16 && (i + j) < length; j++) {
                if (data[i + j] < 16) line += "0";
                line += String(data[i + j], HEX) + " ";
            }
            line += " | ";
            for (size_t j = 0; j < 16 && (i + j) < length; j++) {
                char c = data[i + j];
                line += (c >= 32 && c <= 126) ? String(c) : ".";
            }
            debugPrint(line);
        }
        debugPrint("=== END HEX DUMP ===");
    }
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    bool performWebSocketHandshake() {
        debugPrint("🤝 Starting WebSocket handshake...");
        
        String request = "GET /nt/ws HTTP/1.1\r\n";
        request += "Host: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                   String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort) + "\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: dGVlbnN5LW50LWNsaWVudA==\r\n";
        request += "Sec-WebSocket-Version: 13\r\n";
        request += "Sec-WebSocket-Protocol: networktables.first.wpi.edu\r\n";
        request += "\r\n";
        
        debugPrint("📤 Sending handshake request:");
        debugPrint(request);
        
        client.print(request);
        client.flush();
        
        // Wait for response
        unsigned long startTime = millis();
        String response = "";
        
        while (millis() - startTime < 5000) {
            if (client.available()) {
                response += (char)client.read();
                if (response.endsWith("\r\n\r\n")) break;
            }
            delay(1);
        }
        
        debugPrint("📥 Handshake response:");
        debugPrint(response);
        
        if (response.indexOf("101") >= 0) {
            debugPrint("✅ WebSocket handshake successful");
            return true;
        } else {
            debugPrint("❌ Handshake failed");
            return false;
        }
    }
    
    void sendMaskedMessage(const String& message) {
        if (!isConnected || !client.connected()) {
            debugPrint("❌ Cannot send - not connected");
            return;
        }
        
        debugPrint("📤 Preparing to send message: " + message);
        
        size_t msgLen = message.length();
        
        // Create complete frame in buffer for hex dump
        uint8_t* frame = new uint8_t[6 + msgLen]; // header + mask + message
        size_t framePos = 0;
        
        // Frame header
        frame[framePos++] = 0x81; // Text frame, final fragment
        frame[framePos++] = (uint8_t)(msgLen | 0x80); // Length with mask bit
        
        // Generate masking key
        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) {
            maskKey[i] = random(256);
            frame[framePos++] = maskKey[i];
        }
        
        // Masked payload
        for (size_t i = 0; i < msgLen; i++) {
            frame[framePos++] = message[i] ^ maskKey[i % 4];
        }
        
        // Show what we're sending
        hexDump(frame, framePos, "OUTGOING WEBSOCKET FRAME");
        
        // Send it
        if (client.write(frame, framePos) == framePos) {
            client.flush();
            messagesSent++;
            debugPrint("✅ Message sent successfully (#" + String(messagesSent) + ")");
        } else {
            debugPrint("❌ Failed to send message");
        }
        
        delete[] frame;
    }
    
    void processIncomingData() {
        if (!client.available()) return;
        
        debugPrint("📥 Data available from robot: " + String(client.available()) + " bytes");
        
        // Read all available data
        uint8_t buffer[1024];
        size_t bytesRead = 0;
        
        while (client.available() && bytesRead < sizeof(buffer)) {
            buffer[bytesRead++] = client.read();
        }
        
        if (bytesRead > 0) {
            messagesReceived++;
            debugPrint("📥 Received " + String(bytesRead) + " bytes (#" + String(messagesReceived) + ")");
            
            // Show raw data
            hexDump(buffer, bytesRead, "INCOMING DATA");
            
            // Try to parse as WebSocket frames
            parseWebSocketFrames(buffer, bytesRead);
        }
    }
    
    void parseWebSocketFrames(const uint8_t* data, size_t length) {
        debugPrint("🔍 Parsing WebSocket frames...");
        
        size_t pos = 0;
        while (pos < length) {
            if (pos + 2 > length) {
                debugPrint("❌ Incomplete frame header");
                break;
            }
            
            uint8_t firstByte = data[pos++];
            uint8_t secondByte = data[pos++];
            
            bool fin = (firstByte & 0x80) != 0;
            uint8_t opcode = firstByte & 0x0F;
            bool masked = (secondByte & 0x80) != 0;
            uint8_t payloadLen = secondByte & 0x7F;
            
            debugPrint("📋 Frame: FIN=" + String(fin) + ", Opcode=" + String(opcode) + 
                      ", Masked=" + String(masked) + ", PayloadLen=" + String(payloadLen));
            
            if (opcode == 1) { // Text frame
                if (payloadLen == 0) {
                    debugPrint("📄 Empty text frame");
                    continue;
                }
                
                if (pos + payloadLen > length) {
                    debugPrint("❌ Incomplete payload (need " + String(payloadLen) + 
                              " bytes, have " + String(length - pos) + ")");
                    break;
                }
                
                // Extract text payload
                String payload = "";
                for (int i = 0; i < payloadLen; i++) {
                    payload += (char)data[pos + i];
                }
                pos += payloadLen;
                
                debugPrint("📄 Text payload: " + payload);
                
                // Try to parse as JSON
                parseNTMessage(payload);
                
            } else if (opcode == 8) { // Close frame
                debugPrint("❌ Received close frame from robot");
                
            } else if (opcode == 9) { // Ping frame
                debugPrint("🏓 Received ping frame");
                
            } else if (opcode == 10) { // Pong frame
                debugPrint("🏓 Received pong frame");
                
            } else {
                debugPrint("❓ Unknown opcode: " + String(opcode));
                pos += payloadLen; // Skip unknown frame
            }
        }
    }
    
    void parseNTMessage(const String& message) {
        debugPrint("🔍 Parsing NT message: " + message);
        
        // Simple JSON detection
        if (message.startsWith("[") && message.endsWith("]")) {
            debugPrint("✅ Looks like NT4 JSON array");
            
            // Look for key patterns
            if (message.indexOf("\"method\"") >= 0) {
                debugPrint("📋 Contains method field");
                
                if (message.indexOf("announce") >= 0) {
                    debugPrint("📢 ANNOUNCE message detected");
                } else if (message.indexOf("unannounce") >= 0) {
                    debugPrint("📢 UNANNOUNCE message detected");
                } else if (message.indexOf("setValues") >= 0) {
                    debugPrint("📊 SET VALUES message detected");
                } else if (message.indexOf("update") >= 0) {
                    debugPrint("📊 UPDATE message detected");
                }
            }
            
            // Look for data patterns
            if (message.indexOf("SmartDashboard") >= 0) {
                debugPrint("🎯 SmartDashboard data found!");
            }
            if (message.indexOf("ControlBoard") >= 0) {
                debugPrint("🎯 ControlBoard data found!");
            }
            if (message.indexOf("BatteryVoltage") >= 0) {
                debugPrint("🔋 BatteryVoltage found!");
            }
            
        } else {
            debugPrint("❓ Not recognized as NT4 JSON");
        }
    }
    
public:
    NT4ProtocolDebugger(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        lastStatusPrint(0),
        hasTriedSubscription(false),
        messagesSent(0),
        messagesReceived(0)
    {
    }
    
    bool connect() {
        debugPrint("=== NT4 PROTOCOL DEBUGGER ===");
        debugPrint("Target: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                  String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort));
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connection established");
            
            if (performWebSocketHandshake()) {
                isConnected = true;
                connectionStartTime = millis();
                hasTriedSubscription = false;
                messagesSent = 0;
                messagesReceived = 0;
                debugPrint("🎉 Connection ready!");
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
        
        // Check connection
        if (!client.connected()) {
            debugPrint("💔 Connection lost after " + String(uptime) + "ms");
            isConnected = false;
            return;
        }
        
        // Process incoming data
        processIncomingData();
        
        // Status updates
        if (now - lastStatusPrint >= 10000) { // Every 10 seconds
            debugPrint("💓 Status: Uptime=" + String(uptime) + "ms, Sent=" + 
                      String(messagesSent) + ", Received=" + String(messagesReceived));
            lastStatusPrint = now;
        }
        
        // Send test subscriptions
        if (!hasTriedSubscription && uptime > 5000) {
            debugPrint("📡 Sending test subscriptions...");
            
            // Try specific subscription first
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/BatteryVoltage\"],\"subuid\":1,\"options\":{\"periodic\":0.1}}}]");
            delay(1000);
            
            // Try broader subscription
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/\"],\"subuid\":2,\"options\":{\"periodic\":0.1}}}]");
            delay(1000);
            
            // Try even broader
            sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/\"],\"subuid\":3,\"options\":{\"periodic\":0.1}}}]");
            
            hasTriedSubscription = true;
        }
    }
    
    bool connected() const { return isConnected; }
    
    void printStats() {
        debugPrint("=== STATISTICS ===");
        debugPrint("Messages sent: " + String(messagesSent));
        debugPrint("Messages received: " + String(messagesReceived));
        debugPrint("Connection uptime: " + String(millis() - connectionStartTime) + "ms");
    }
};

#endif // NT_PROTOCOL_DEBUGGER_H