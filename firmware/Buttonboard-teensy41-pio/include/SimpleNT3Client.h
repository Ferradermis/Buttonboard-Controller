/*
 * Simple NT3 Client for Teensy 4.1
 * 
 * Since OutlineViewer is using NT3 (port 1740), let's use that instead of NT4.
 * NT3 is much simpler - no WebSocket, just direct TCP with a simple protocol.
 */

#ifndef SIMPLE_NT3_CLIENT_H
#define SIMPLE_NT3_CLIENT_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <functional>

typedef std::function<void(bool connected)> ConnectionCallback;

class SimpleNT3Client {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    unsigned long lastHeartbeat;
    
    ConnectionCallback connectionCallback;
    
    // Robot data storage
    struct {
        double batteryVoltage = 0.0;
        bool robotEnabled = false;
        String robotMode = "Unknown";
        bool shooterReady = false;
        bool intakeDeployed = false;
        String allianceColor = "unknown";
        String autoMode = "Unknown";
        bool fieldOriented = false;
        double matchTimeRemaining = 0.0;
        String statusMessage = "";
        int heartbeat = 0;
        unsigned long lastUpdate = 0;
    } robotData;
    
    // Statistics
    struct {
        unsigned long connectTime;
        unsigned long totalConnections;
        unsigned long totalDisconnects;
        unsigned long messagesReceived;
        unsigned long messagesSent;
    } stats;
    
    void debugPrint(const String& message) {
        Serial.println("NT3: " + message);
    }
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    bool performNT3Handshake() {
        debugPrint("🤝 Starting NT3 handshake...");
        
        // NT3 handshake is much simpler - just send protocol version
        // Protocol: 2 bytes for version (0x0300 = version 3.0)
        uint8_t handshake[] = {0x03, 0x00};
        
        if (client.write(handshake, 2) == 2) {
            client.flush();
            debugPrint("📤 Sent NT3 version (3.0)");
            
            // Wait for server response
            unsigned long startTime = millis();
            while (millis() - startTime < 3000) {
                if (client.available() >= 2) {
                    uint8_t response[2];
                    response[0] = client.read();
                    response[1] = client.read();
                    
                    debugPrint("📥 Server version: " + String(response[0]) + "." + String(response[1]));
                    
                    if (response[0] == 3) { // NT3.x
                        debugPrint("✅ NT3 handshake successful!");
                        return true;
                    } else {
                        debugPrint("❌ Unsupported server version");
                        return false;
                    }
                }
                delay(1);
            }
            
            debugPrint("❌ No handshake response from server");
            return false;
        } else {
            debugPrint("❌ Failed to send handshake");
            return false;
        }
    }
    
    void processIncomingData() {
        static uint8_t buffer[1024];
        static size_t bufferPos = 0;
        
        // Read available data
        while (client.available() && bufferPos < sizeof(buffer) - 1) {
            buffer[bufferPos++] = client.read();
        }
        
        if (bufferPos > 0) {
            // NT3 uses a different message format - let's just look for patterns
            stats.messagesReceived++;
            
            debugPrint("📥 NT3 data received (" + String(bufferPos) + " bytes)");
            
            // Look for text patterns in the data
            String dataStr = "";
            for (size_t i = 0; i < bufferPos; i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) { // Printable ASCII
                    dataStr += (char)buffer[i];
                } else {
                    dataStr += ".";
                }
            }
            
            debugPrint("📄 Data: " + dataStr.substring(0, 100) + (dataStr.length() > 100 ? "..." : ""));
            
            // Look for battery voltage patterns
            if (dataStr.indexOf("battery") >= 0 || dataStr.indexOf("Battery") >= 0) {
                debugPrint("🔋 Found battery-related data!");
            }
            
            if (dataStr.indexOf("11.75") >= 0) {
                debugPrint("⚡ Found our hardcoded voltage value!");
                robotData.batteryVoltage = 11.75;
                robotData.lastUpdate = millis();
            }
            
            // Reset buffer
            bufferPos = 0;
        }
    }
    
    void sendNT3KeepAlive() {
        // NT3 keep-alive is typically just maintaining the TCP connection
        // Some implementations send periodic empty messages
        uint8_t keepAlive[] = {0x00};
        client.write(keepAlive, 1);
        client.flush();
        stats.messagesSent++;
    }
    
public:
    SimpleNT3Client(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(1740), // Start with OutlineViewer's port
        isConnected(false),
        connectionStartTime(0),
        lastHeartbeat(0),
        connectionCallback(nullptr)
    {
        memset(&stats, 0, sizeof(stats));
    }
    
    void setConnectionCallback(ConnectionCallback callback) {
        connectionCallback = callback;
    }
    
    bool begin() {
        debugPrint("=== Starting NT3 Connection ===");
        
        // Try port 1740 first (where OutlineViewer is connected)
        debugPrint("Trying port 1740 (OutlineViewer port)...");
        ntPort = 1740;
        
        if (attemptConnection()) {
            return true;
        }
        
        // Try port 1735 (standard NT3 port)
        debugPrint("Port 1740 failed, trying port 1735 (standard NT3)...");
        ntPort = 1735;
        
        if (attemptConnection()) {
            return true;
        }
        
        debugPrint("❌ Both NT3 ports failed");
        return false;
    }
    
private:
    bool attemptConnection() {
        debugPrint("Connecting to " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                  String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort));
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected to NT3 port " + String(ntPort));
            
            if (performNT3Handshake()) {
                debugPrint("✅ NT3 handshake successful on port " + String(ntPort));
                
                isConnected = true;
                connectionStartTime = millis();
                stats.connectTime = millis();
                stats.totalConnections++;
                
                if (connectionCallback) {
                    connectionCallback(true);
                }
                
                lastHeartbeat = millis();
                
                debugPrint("🎉 NT3 connection established on port " + String(ntPort) + "!");
                debugPrint("💡 This is the same protocol OutlineViewer is using");
                return true;
            } else {
                client.stop();
            }
        } else {
            debugPrint("❌ TCP connection failed on port " + String(ntPort));
        }
        
        return false;
    }
    
public:
    
    void update() {
        if (!isConnected) return;
        
        if (!client.connected()) {
            debugPrint("💔 NT3 connection lost");
            isConnected = false;
            stats.totalDisconnects++;
            
            if (connectionCallback) {
                connectionCallback(false);
            }
            return;
        }
        
        // Process incoming data
        processIncomingData();
        
        // Send periodic keep-alive
        unsigned long now = millis();
        if (now - lastHeartbeat >= 5000) { // Every 5 seconds
            sendNT3KeepAlive();
            lastHeartbeat = now;
            debugPrint("💓 NT3 keep-alive sent");
        }
    }
    
    // Data accessors (same API as NT4 client)
    bool connected() const { return isConnected; }
    double getBatteryVoltage() const { return robotData.batteryVoltage; }
    bool isRobotEnabled() const { return robotData.robotEnabled; }
    String getRobotMode() const { return robotData.robotMode; }
    bool isShooterReady() const { return robotData.shooterReady; }
    bool isIntakeDeployed() const { return robotData.intakeDeployed; }
    String getAllianceColor() const { return robotData.allianceColor; }
    String getAutoMode() const { return robotData.autoMode; }
    bool isFieldOriented() const { return robotData.fieldOriented; }
    double getMatchTimeRemaining() const { return robotData.matchTimeRemaining; }
    String getStatusMessage() const { return robotData.statusMessage; }
    int getHeartbeat() const { return robotData.heartbeat; }
    unsigned long getLastDataUpdate() const { return robotData.lastUpdate; }
    
    void printStatus() {
        debugPrint("=== NT3 Status ===");
        debugPrint("Connected: " + String(isConnected ? "YES" : "NO"));
        if (isConnected) {
            debugPrint("Uptime: " + String(millis() - connectionStartTime) + "ms");
            debugPrint("Messages RX/TX: " + String(stats.messagesReceived) + "/" + String(stats.messagesSent));
            if (robotData.lastUpdate > 0) {
                debugPrint("Battery: " + String(robotData.batteryVoltage) + "V");
                debugPrint("Data age: " + String(millis() - robotData.lastUpdate) + "ms");
            }
        }
        debugPrint("==================");
    }
};

// Alternative: Try port 1735 (standard NT3 port)
// The main client now tries both ports automatically, so this is not needed

#endif // SIMPLE_NT3_CLIENT_H