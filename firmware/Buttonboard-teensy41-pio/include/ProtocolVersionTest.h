/*
 * Protocol Version Test
 * 
 * Since we get 0 topics on NT4 (port 5810), let's test if the robot
 * is actually running NT3 (port 1735) instead.
 */

#ifndef PROTOCOL_VERSION_TEST_H
#define PROTOCOL_VERSION_TEST_H

#include <Arduino.h>
#include <NativeEthernet.h>

class ProtocolVersionTest {
private:
    IPAddress robotIP;
    
    void debugPrint(const String& message) {
        Serial.println("[" + String(millis()) + "] TEST: " + message);
    }
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    bool testPort(uint16_t port, const String& protocol) {
        debugPrint("🔍 Testing " + protocol + " on port " + String(port) + "...");
        
        EthernetClient client;
        
        if (client.connect(robotIP, port)) {
            debugPrint("✅ TCP connection successful on port " + String(port));
            
            if (port == 5810) {
                // Test NT4 WebSocket handshake
                String request = "GET /nt/ws HTTP/1.1\r\n";
                request += "Host: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                           String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(port) + "\r\n";
                request += "Upgrade: websocket\r\n";
                request += "Connection: Upgrade\r\n";
                request += "Sec-WebSocket-Key: dGVlbnN5LW50LWNsaWVudA==\r\n";
                request += "Sec-WebSocket-Version: 13\r\n";
                request += "Sec-WebSocket-Protocol: networktables.first.wpi.edu\r\n";
                request += "\r\n";
                
                client.print(request);
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
                    debugPrint("✅ NT4 WebSocket handshake successful");
                    
                    // Try to get some data
                    debugPrint("🔍 Listening for NT4 data for 5 seconds...");
                    unsigned long listenStart = millis();
                    bool gotData = false;
                    
                    while (millis() - listenStart < 5000) {
                        if (client.available()) {
                            gotData = true;
                            debugPrint("📥 NT4 data received!");
                            break;
                        }
                        delay(100);
                    }
                    
                    if (!gotData) {
                        debugPrint("❌ NT4: No data received in 5 seconds");
                    }
                    
                } else {
                    debugPrint("❌ NT4 WebSocket handshake failed");
                    debugPrint("Response: " + response.substring(0, 100));
                }
                
            } else if (port == 1735) {
                // Test NT3 - just listen for any data
                debugPrint("🔍 Listening for NT3 data for 5 seconds...");
                unsigned long listenStart = millis();
                bool gotData = false;
                int bytesReceived = 0;
                
                while (millis() - listenStart < 5000) {
                    if (client.available()) {
                        gotData = true;
                        bytesReceived++;
                        client.read(); // Just count bytes
                    }
                    delay(10);
                }
                
                if (gotData) {
                    debugPrint("✅ NT3: Received " + String(bytesReceived) + " bytes of data!");
                } else {
                    debugPrint("❌ NT3: No data received in 5 seconds");
                }
            }
            
            client.stop();
            return true;
            
        } else {
            debugPrint("❌ TCP connection failed on port " + String(port));
            return false;
        }
    }
    
public:
    ProtocolVersionTest(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber))
    {
    }
    
    void runAllTests() {
        debugPrint("=== NETWORKTABLES PROTOCOL VERSION TEST ===");
        debugPrint("Robot IP: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                  String(robotIP[2]) + "." + String(robotIP[3]));
        
        // Test common robot ports first
        debugPrint("\n🔍 Testing robot connectivity...");
        EthernetClient testClient;
        
        // Test SSH (should always work if robot is on)
        if (testClient.connect(robotIP, 22)) {
            debugPrint("✅ Robot is reachable (SSH port 22)");
            testClient.stop();
        } else {
            debugPrint("❌ Robot unreachable on SSH port 22");
            return;
        }
        
        // Test HTTP (roborio web interface)
        if (testClient.connect(robotIP, 80)) {
            debugPrint("✅ Robot web interface accessible (port 80)");
            testClient.stop();
        } else {
            debugPrint("⚠️  Robot web interface not accessible (port 80)");
        }
        
        debugPrint("\n🧪 Testing NetworkTables protocols...");
        
        // Test NT4 (port 5810)
        bool nt4Works = testPort(5810, "NT4");
        
        // Test NT3 (port 1735) 
        bool nt3Works = testPort(1735, "NT3");
        
        // Test other common ports
        debugPrint("\n🔍 Testing other common FRC ports...");
        
        uint16_t otherPorts[] = {5800, 5801, 5802, 5805, 1180, 1181, 1182, 1183};
        String portNames[] = {"Camera", "Camera", "Camera", "CameraSettings", "DS", "DS", "DS", "DS"};
        
        for (int i = 0; i < 8; i++) {
            if (testClient.connect(robotIP, otherPorts[i])) {
                debugPrint("✅ " + portNames[i] + " port " + String(otherPorts[i]) + " is open");
                testClient.stop();
            }
        }
        
        // Print summary
        debugPrint("\n=== SUMMARY ===");
        if (nt4Works && nt3Works) {
            debugPrint("✅ Both NT3 and NT4 ports are accessible");
            debugPrint("💡 Robot may be running both protocols");
        } else if (nt4Works) {
            debugPrint("✅ Only NT4 (port 5810) is working");
            debugPrint("💡 Robot is running NT4 - check subscription format");
        } else if (nt3Works) {
            debugPrint("✅ Only NT3 (port 1735) is working");
            debugPrint("💡 Robot is running NT3, not NT4!");
            debugPrint("🔧 SOLUTION: Use NT3 client instead of NT4");
        } else {
            debugPrint("❌ Neither NT3 nor NT4 ports are working");
            debugPrint("💡 NetworkTables server may not be running");
        }
        
        debugPrint("===============");
    }
};

#endif // PROTOCOL_VERSION_TEST_H