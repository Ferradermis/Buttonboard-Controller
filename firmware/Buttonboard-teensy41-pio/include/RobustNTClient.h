/*
 * FIXED VERSION: Robust NetworkTables Client for Teensy 4.1
 * 
 * Key fixes for connection stability:
 * 1. Proper NT4 WebSocket frame handling
 * 2. Better connection timing and delays
 * 3. Simplified message protocol
 * 4. More robust reconnection logic
 */

#ifndef ROBUST_NT_CLIENT_H
#define ROBUST_NT_CLIENT_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>
#include <map>
#include <functional>

// Callback types
typedef std::function<void(const String& key, double value)> NumberCallback;
typedef std::function<void(const String& key, bool value)> BooleanCallback;
typedef std::function<void(const String& key, const String& value)> StringCallback;
typedef std::function<void(bool connected)> ConnectionCallback;

class RobustNTClient {
private:
    int teamNumber;
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    bool autoReconnect;
    unsigned long lastConnectionAttempt;
    unsigned long connectionTimeout;
    unsigned long heartbeatInterval;
    unsigned long lastHeartbeat;
    unsigned long lastDataReceived;
    unsigned long connectionStartTime;
    
    // Connection stability
    int consecutiveFailures;
    int maxConsecutiveFailures;
    bool debugMode;
    bool subscriptionSent;
    
    ConnectionCallback connectionCallback;
    
    // Simple data storage for robot telemetry
    struct {
        // Battery data
        double batteryVoltage = 0.0;
        bool batteryIsLow = false;
        
        // Robot state
        bool robotEnabled = false;
        bool isAutonomous = false;
        bool isTeleop = false;
        String robotMode = "Unknown";
        
        // Alliance info
        String allianceColor = "unknown";
        bool isRedAlliance = false;
        
        // Subsystem data
        double shooterSpeed = 0.0;
        bool shooterReady = false;
        bool intakeDeployed = false;
        double intakePosition = 0.0;
        
        // Auto selection
        String autoMode = "Unknown";
        int autoModeNumber = 0;
        
        // Drive state
        bool fieldOriented = false;
        
        // Match data
        double matchTimeRemaining = 0.0;
        
        // Status
        String statusMessage = "";
        int heartbeat = 0;
        
        unsigned long lastUpdate = 0;
    } robotData;
    
    // Subscription ID management
    int nextSubId;
    int nextPubId;
    
    // Statistics
    struct {
        unsigned long connectTime;
        unsigned long totalConnections;
        unsigned long totalDisconnects;
        unsigned long messagesReceived;
        unsigned long messagesSent;
    } stats;
    
    IPAddress calculateRobotIP(int teamNumber);
    void debugPrint(const String& message);
    bool testBasicConnectivity();
    void handleConnectionLoss();
    bool performWebSocketHandshake();
    void sendNT4Message(const String& message);
    void processWebSocketData();
    void parseNT4Message(const String& message);
    void parseControlBoardData(const String& topic, JsonVariant value);
    void parseSmartDashboardData(const String& topic, JsonVariant value);
    
public:
    RobustNTClient(int teamNumber = 0);
    ~RobustNTClient();
    
    // Configuration
    void setTeamNumber(int team);
    void setDebugMode(bool enable) { debugMode = enable; }
    void setConnectionTimeout(unsigned long timeout) { connectionTimeout = timeout; }
    void setConnectionCallback(ConnectionCallback callback);
    
    // Connection management
    bool begin();
    bool begin(IPAddress customIP, uint16_t port = 5810);
    void update();
    
    // Status
    bool connected() const { return isConnected; }
    unsigned long getUptime() const;
    void getStatistics(unsigned long& connections, unsigned long& disconnects, 
                      unsigned long& messagesRx, unsigned long& messagesTx);
    
    // Publishing (optional - for sending data to robot)
    bool publishNumber(const String& key, double value);
    bool publishBoolean(const String& key, bool value);
    bool publishString(const String& key, const String& value);
    bool publishHeartbeat();
    bool requestRobotData();
    
    // Basic data access
    double getBatteryVoltage() const { return robotData.batteryVoltage; }
    bool isRobotEnabled() const { return robotData.robotEnabled; }
    bool isShooterReady() const { return robotData.shooterReady; }
    bool isIntakeDeployed() const { return robotData.intakeDeployed; }
    String getAllianceColor() const { return robotData.allianceColor; }
    String getAutoMode() const { return robotData.autoMode; }
    String getRobotMode() const { return robotData.robotMode; }
    bool isFieldOriented() const { return robotData.fieldOriented; }
    double getMatchTimeRemaining() const { return robotData.matchTimeRemaining; }
    String getStatusMessage() const { return robotData.statusMessage; }
    int getHeartbeat() const { return robotData.heartbeat; }
    unsigned long getLastDataUpdate() const { return robotData.lastUpdate; }
    
    // Diagnostics
    void printStatus();
    void printConnectionDiagnostics();
    void runConnectionTest();
    void runBasicTest(); // Simplified test
};

RobustNTClient::RobustNTClient(int teamNumber) :
    teamNumber(teamNumber),
    ntPort(5810),
    isConnected(false),
    autoReconnect(true),
    lastConnectionAttempt(0),
    connectionTimeout(10000), // Increased timeout
    heartbeatInterval(5000),  // Reduced frequency
    lastHeartbeat(0),
    lastDataReceived(0),
    connectionStartTime(0),
    consecutiveFailures(0),
    maxConsecutiveFailures(3), // Reduced max attempts
    debugMode(true),
    subscriptionSent(false),
    connectionCallback(nullptr),
    nextSubId(1),
    nextPubId(1)
{
    robotIP = calculateRobotIP(teamNumber);
    memset(&stats, 0, sizeof(stats));
}

RobustNTClient::~RobustNTClient() {
    if (client.connected()) {
        client.stop();
    }
}

void RobustNTClient::setTeamNumber(int team) {
    teamNumber = team;
    robotIP = calculateRobotIP(team);
}

void RobustNTClient::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback = callback;
}

bool RobustNTClient::begin() {
    if (teamNumber == 0) {
        debugPrint("Error: Team number not set");
        return false;
    }
    return begin(robotIP, ntPort);
}

bool RobustNTClient::begin(IPAddress customIP, uint16_t port) {
    robotIP = customIP;
    ntPort = port;
    
    debugPrint("=== Starting NT4 Connection ===");
    debugPrint("Target: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
               String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort));
    
    // Reset state
    isConnected = false;
    subscriptionSent = false;
    
    // Test basic connectivity first
    if (!testBasicConnectivity()) {
        debugPrint("❌ Basic connectivity test failed");
        consecutiveFailures++;
        lastConnectionAttempt = millis();
        return false;
    }
    
    // Try to connect with longer timeout
    debugPrint("🔌 Attempting TCP connection...");
    unsigned long startTime = millis();
    
    if (client.connect(robotIP, ntPort)) {
        unsigned long connectTime = millis() - startTime;
        debugPrint("✅ TCP connected in " + String(connectTime) + "ms");
        
        // Wait a bit for connection to stabilize
        delay(100);
        
        // Perform WebSocket handshake
        if (performWebSocketHandshake()) {
            debugPrint("✅ WebSocket handshake successful");
            
            // Connection is established - wait before marking as ready
            delay(500); // Give robot time to process
            
            isConnected = true;
            connectionStartTime = millis();
            stats.connectTime = millis();
            stats.totalConnections++;
            consecutiveFailures = 0;
            
            debugPrint("🎉 NT4 connection fully established!");
            
            if (connectionCallback) {
                connectionCallback(true);
            }
            
            lastConnectionAttempt = millis();
            lastDataReceived = millis();
            return true;
            
        } else {
            debugPrint("❌ WebSocket handshake failed");
            client.stop();
        }
    } else {
        debugPrint("❌ TCP connection failed");
    }
    
    consecutiveFailures++;
    lastConnectionAttempt = millis();
    return false;
}

bool RobustNTClient::performWebSocketHandshake() {
    debugPrint("🤝 Starting WebSocket handshake...");
    
    // Send HTTP upgrade request - simplified and more compatible
    String request = "";
    request += "GET /nt/ws HTTP/1.1\r\n";
    request += "Host: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
               String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort) + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: dGVlbnN5LW50LWNsaWVudA==\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "Sec-WebSocket-Protocol: networktables.first.wpi.edu\r\n";
    request += "\r\n";
    
    client.print(request);
    client.flush(); // Ensure it's sent immediately
    
    debugPrint("📤 Sent handshake request");
    
    // Wait for response with timeout
    unsigned long startTime = millis();
    String response = "";
    bool headerComplete = false;
    
    while (millis() - startTime < 5000 && !headerComplete) { // 5 second timeout
        if (client.available()) {
            char c = client.read();
            response += c;
            
            // Look for end of HTTP headers
            if (response.endsWith("\r\n\r\n")) {
                headerComplete = true;
            }
        }
        delay(1);
    }
    
    if (!headerComplete) {
        debugPrint("❌ Handshake timeout - no response");
        return false;
    }
    
    debugPrint("📥 Handshake response received (" + String(response.length()) + " bytes)");
    
    // Check for successful upgrade - be more lenient
    if (response.indexOf("101") >= 0 && response.indexOf("websocket") >= 0) {
        debugPrint("✅ WebSocket upgrade successful");
        return true;
    } else {
        debugPrint("❌ WebSocket upgrade failed");
        // Print first part of response for debugging
        int printLen = min(200, (int)response.length());
        debugPrint("Response start: " + response.substring(0, printLen));
        return false;
    }
}

void RobustNTClient::update() {
    unsigned long currentTime = millis();
    
    // Check connection status
    if (isConnected) {
        if (!client.connected()) {
            debugPrint("💔 TCP connection lost");
            handleConnectionLoss();
            return;
        }
        
        // Process incoming data
        if (client.available()) {
            processWebSocketData();
        }
        
        // Send subscription request after connection is stable (only once)
        if (!subscriptionSent && (currentTime - connectionStartTime) > 2000) {
            debugPrint("📡 Sending subscription request...");
            requestRobotData();
            subscriptionSent = true;
        }
        
        // Periodic heartbeat (less frequent to avoid overwhelming)
        if (currentTime - lastHeartbeat >= heartbeatInterval) {
            debugPrint("💓 Connection alive for " + String(getUptime()) + "ms");
            lastHeartbeat = currentTime;
            
            // Check if we've received any data recently
            if (subscriptionSent && (currentTime - lastDataReceived) > 10000) {
                debugPrint("⚠️  No data received for 10+ seconds");
            }
        }
    }
    
    // Auto-reconnect logic with exponential backoff
    if (!isConnected && autoReconnect) {
        unsigned long retryInterval = 2000 + (consecutiveFailures * 3000); // Start at 2s, increase by 3s each failure
        retryInterval = min(retryInterval, 20000UL); // Max 20 seconds
        
        if (currentTime - lastConnectionAttempt >= retryInterval) {
            if (consecutiveFailures < maxConsecutiveFailures) {
                debugPrint("🔄 Reconnection attempt " + String(consecutiveFailures + 1) + 
                          "/" + String(maxConsecutiveFailures));
                begin(robotIP, ntPort);
            } else {
                debugPrint("⏸️  Max attempts reached - pausing for 30s");
                consecutiveFailures = 0; // Reset counter
                lastConnectionAttempt = currentTime + 30000; // Wait 30 seconds
            }
        }
    }
}

bool RobustNTClient::testBasicConnectivity() {
    debugPrint("🔍 Testing robot connectivity...");
    
    EthernetClient testClient;
    
    // Test NetworkTables port specifically
    if (testClient.connect(robotIP, 5810)) {
        testClient.stop();
        debugPrint("✅ NetworkTables port is accessible");
        return true;
    }
    
    // Test other common ports as backup
    uint16_t testPorts[] = {22, 80, 1735};
    for (int i = 0; i < 3; i++) {
        if (testClient.connect(robotIP, testPorts[i])) {
            testClient.stop();
            debugPrint("✅ Robot reachable on port " + String(testPorts[i]));
            return true;
        }
    }
    
    debugPrint("❌ Robot unreachable on all tested ports");
    return false;
}

void RobustNTClient::handleConnectionLoss() {
    if (isConnected) {
        isConnected = false;
        subscriptionSent = false;
        stats.totalDisconnects++;
        
        debugPrint("💔 Connection lost after " + String(getUptime()) + "ms");
        
        if (connectionCallback) {
            connectionCallback(false);
        }
        
        client.stop();
    }
}

bool RobustNTClient::requestRobotData() {
    if (isConnected && client.connected()) {
        // Send simplified subscription for all SmartDashboard data
        String subMessage = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/\"],\"subuid\":" + 
                           String(nextSubId++) + ",\"options\":{\"periodic\":0.1,\"all\":false}}}]";
        
        debugPrint("📡 Subscribing to SmartDashboard data...");
        sendNT4Message(subMessage);
        
        // Also subscribe to ControlBoard data
        delay(100);
        String cbSubMessage = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"ControlBoard/\"],\"subuid\":" + 
                             String(nextSubId++) + ",\"options\":{\"periodic\":0.1,\"all\":false}}}]";
        
        debugPrint("📡 Subscribing to ControlBoard data...");
        sendNT4Message(cbSubMessage);
        
        return true;
    }
    return false;
}

void RobustNTClient::sendNT4Message(const String& message) {
    if (!isConnected || !client.connected()) {
        debugPrint("❌ Cannot send - not connected");
        return;
    }
    
    size_t msgLen = message.length();
    
    // WebSocket clients MUST mask data sent to servers - this was the problem!
    
    // Frame header
    if (client.write(0x81) != 1) { // Text frame, final fragment
        debugPrint("❌ Failed to write frame header");
        return;
    }
    
    // Payload length with MASK bit set (0x80) - CRITICAL!
    if (msgLen < 126) {
        if (client.write((uint8_t)(msgLen | 0x80)) != 1) { // Set mask bit
            debugPrint("❌ Failed to write payload length");
            return;
        }
    } else {
        debugPrint("❌ Message too long: " + String(msgLen));
        return;
    }
    
    // Generate 4-byte masking key (required by WebSocket spec)
    uint8_t maskKey[4];
    maskKey[0] = random(256);
    maskKey[1] = random(256);
    maskKey[2] = random(256);
    maskKey[3] = random(256);
    
    // Send masking key
    if (client.write(maskKey, 4) != 4) {
        debugPrint("❌ Failed to write masking key");
        return;
    }
    
    // Send masked payload data (XOR each byte with rotating mask key)
    for (size_t i = 0; i < msgLen; i++) {
        uint8_t maskedByte = message[i] ^ maskKey[i % 4];
        if (client.write(maskedByte) != 1) {
            debugPrint("❌ Failed to write masked byte at position " + String(i));
            return;
        }
    }
    
    client.flush();
    stats.messagesSent++;
    
    debugPrint("📤 Sent MASKED (" + String(msgLen) + "b): " + 
              message.substring(0, 60) + (message.length() > 60 ? "..." : ""));
}

void RobustNTClient::processWebSocketData() {
    static String frameBuffer = "";
    static bool inFrame = false;
    static uint8_t expectedLen = 0;
    
    while (client.available()) {
        uint8_t byte = client.read();
        
        if (!inFrame) {
            // Look for text frame start
            if (byte == 0x81) {
                inFrame = true;
                frameBuffer = "";
                expectedLen = 0;
            }
        } else if (expectedLen == 0) {
            // Read payload length
            expectedLen = byte;
            if (expectedLen == 0) {
                inFrame = false; // Empty frame
            }
        } else {
            // Read payload
            frameBuffer += (char)byte;
            
            if (frameBuffer.length() >= expectedLen) {
                // Complete frame received
                lastDataReceived = millis();
                stats.messagesReceived++;
                
                debugPrint("📥 Received (" + String(frameBuffer.length()) + "b): " + 
                          frameBuffer.substring(0, 60) + (frameBuffer.length() > 60 ? "..." : ""));
                
                parseNT4Message(frameBuffer);
                
                // Reset for next frame
                inFrame = false;
                expectedLen = 0;
                frameBuffer = "";
            }
        }
    }
}

void RobustNTClient::parseNT4Message(const String& message) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        debugPrint("❌ JSON parse error: " + String(error.c_str()));
        return;
    }
    
    if (doc.is<JsonArray>()) {
        JsonArray messageArray = doc.as<JsonArray>();
        
        for (JsonObject messageObj : messageArray) {
            String method = messageObj["method"];
            
            if (method == "announce") {
                String name = messageObj["params"]["name"];
                String type = messageObj["params"]["type"];
                debugPrint("📢 Robot announced: " + name + " (" + type + ")");
                
            } else if (method == "unannounce") {
                String name = messageObj["params"]["name"];
                debugPrint("📢 Robot unannounced: " + name);
                
            } else if (method == "setValues" || method == "update") {
                JsonObject params = messageObj["params"];
                for (JsonPair kv : params) {
                    String topic = kv.key().c_str();
                    JsonVariant value = kv.value();
                    
                    debugPrint("📊 " + topic + " = " + value.as<String>());
                    
                    // Update robot data
                    if (topic.startsWith("ControlBoard/")) {
                        parseControlBoardData(topic, value);
                    } else if (topic.startsWith("SmartDashboard/")) {
                        parseSmartDashboardData(topic, value);
                    }
                }
            }
        }
    }
}

void RobustNTClient::parseControlBoardData(const String& topic, JsonVariant value) {
    robotData.lastUpdate = millis();
    
    if (topic == "ControlBoard/battery/voltage") {
        robotData.batteryVoltage = value.as<double>();
    } else if (topic == "ControlBoard/robot/enabled") {
        robotData.robotEnabled = value.as<bool>();
    } else if (topic == "ControlBoard/robot/mode") {
        robotData.robotMode = value.as<String>();
    } else if (topic == "ControlBoard/shooter/ready") {
        robotData.shooterReady = value.as<bool>();
    } else if (topic == "ControlBoard/alliance/color") {
        robotData.allianceColor = value.as<String>();
    }
}

void RobustNTClient::parseSmartDashboardData(const String& topic, JsonVariant value) {
    robotData.lastUpdate = millis();
    
    if (topic == "SmartDashboard/BatteryVoltage") {
        robotData.batteryVoltage = value.as<double>();
        debugPrint("🔋 Battery: " + String(robotData.batteryVoltage) + "V");
    } else if (topic == "SmartDashboard/RobotEnabled") {
        robotData.robotEnabled = value.as<bool>();
    } else if (topic == "SmartDashboard/ShooterReady") {
        robotData.shooterReady = value.as<bool>();
    }
}

// Utility methods
void RobustNTClient::debugPrint(const String& message) {
    if (debugMode) {
        Serial.println("NT: " + message);
    }
}

IPAddress RobustNTClient::calculateRobotIP(int teamNumber) {
    if (teamNumber <= 0) {
        return IPAddress(10, 0, 0, 2);
    }
    
    int firstOctet = teamNumber / 100;
    int secondOctet = teamNumber % 100;
    return IPAddress(10, firstOctet, secondOctet, 2);
}

unsigned long RobustNTClient::getUptime() const {
    if (!isConnected || stats.connectTime == 0) return 0;
    return millis() - stats.connectTime;
}

void RobustNTClient::printStatus() {
    Serial.println("=== NetworkTables Status ===");
    Serial.printf("Connected: %s\n", isConnected ? "YES" : "NO");
    Serial.printf("Robot IP: %d.%d.%d.%d:%d\n", robotIP[0], robotIP[1], robotIP[2], robotIP[3], ntPort);
    
    if (isConnected) {
        Serial.printf("Uptime: %lu ms\n", getUptime());
        Serial.printf("Subscription sent: %s\n", subscriptionSent ? "YES" : "NO");
        Serial.printf("Last data: %lu ms ago\n", millis() - lastDataReceived);
    }
    
    Serial.printf("Connections/Disconnects: %lu/%lu\n", stats.totalConnections, stats.totalDisconnects);
    Serial.printf("Messages RX/TX: %lu/%lu\n", stats.messagesReceived, stats.messagesSent);
    
    if (robotData.lastUpdate > 0) {
        Serial.printf("Battery: %.1fV (age: %lu ms)\n", robotData.batteryVoltage, millis() - robotData.lastUpdate);
    }
    Serial.println("============================");
}

void RobustNTClient::runBasicTest() {
    debugPrint("=== Basic NT4 Connection Test ===");
    
    if (!isConnected) {
        debugPrint("❌ Not connected - cannot run test");
        return;
    }
    
    debugPrint("✅ Connection established");
    debugPrint("⏱️  Waiting 10 seconds for data...");
    
    unsigned long testStart = millis();
    unsigned long initialRx = stats.messagesReceived;
    
    while (millis() - testStart < 10000) {
        // Just wait and see if we get data
        delay(100);
        
        if (stats.messagesReceived > initialRx) {
            debugPrint("🎉 SUCCESS: Received " + String(stats.messagesReceived - initialRx) + " messages!");
            if (robotData.lastUpdate > 0) {
                debugPrint("🔋 Battery data: " + String(robotData.batteryVoltage) + "V");
            }
            break;
        }
    }
    
    if (stats.messagesReceived == initialRx) {
        debugPrint("⚠️  No messages received - check robot code");
    }
    
    debugPrint("=== Test Complete ===");
}

// Simplified publishing methods
bool RobustNTClient::publishNumber(const String& key, double value) {
    if (isConnected && client.connected()) {
        String msg = "[{\"method\":\"publish\",\"params\":{\"name\":\"" + key + 
                     "\",\"type\":\"double\",\"value\":" + String(value, 6) + "}}]";
        sendNT4Message(msg);
        return true;
    }
    return false;
}

bool RobustNTClient::publishBoolean(const String& key, bool value) {
    if (isConnected && client.connected()) {
        String msg = "[{\"method\":\"publish\",\"params\":{\"name\":\"" + key + 
                     "\",\"type\":\"boolean\",\"value\":" + (value ? "true" : "false") + "}}]";
        sendNT4Message(msg);
        return true;
    }
    return false;
}

bool RobustNTClient::publishString(const String& key, const String& value) {
    if (isConnected && client.connected()) {
        String msg = "[{\"method\":\"publish\",\"params\":{\"name\":\"" + key + 
                     "\",\"type\":\"string\",\"value\":\"" + value + "\"}}]";
        sendNT4Message(msg);
        return true;
    }
    return false;
}

bool RobustNTClient::publishHeartbeat() {
    return publishNumber("SmartDashboard/TeensyHeartbeat", millis());
}

#endif // ROBUST_NT_CLIENT_H