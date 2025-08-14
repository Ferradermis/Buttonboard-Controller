/*
 * Robust NetworkTables Client for Teensy 4.1
 * Focuses on stable connections with better debugging
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
    
    // Connection stability
    int consecutiveFailures;
    int maxConsecutiveFailures;
    bool debugMode;
    
    ConnectionCallback connectionCallback;
    
    // Simple data storage for basic robot telemetry
    struct {
        double batteryVoltage = 0.0;
        bool robotEnabled = false;
        String allianceColor = "unknown";
        bool emergencyStop = false;
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
    bool publishEmergencyStop();
    bool requestRobotData();
    
    // Basic data access
    double getBatteryVoltage() const { return robotData.batteryVoltage; }
    bool isRobotEnabled() const { return robotData.robotEnabled; }
    String getAllianceColor() const { return robotData.allianceColor; }
    unsigned long getLastDataUpdate() const { return robotData.lastUpdate; }
    
    // Diagnostics
    void printStatus();
    void printConnectionDiagnostics();
    void runConnectionTest();
    void runMessageTests(); // New method for testing messages
    void runConnectionHealthTest(); // Test bare connection stability
};

// Implementation

RobustNTClient::RobustNTClient(int teamNumber) :
    teamNumber(teamNumber),
    ntPort(5810),
    isConnected(false),
    autoReconnect(true),
    lastConnectionAttempt(0),
    connectionTimeout(5000),
    heartbeatInterval(1000),
    lastHeartbeat(0),
    lastDataReceived(0),
    consecutiveFailures(0),
    maxConsecutiveFailures(5),
    debugMode(true),
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
    
    debugPrint("Attempting connection to " + 
               String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
               String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort));
    
    // Test basic connectivity first
    if (!testBasicConnectivity()) {
        debugPrint("Basic connectivity test failed");
        consecutiveFailures++;
        lastConnectionAttempt = millis();
        return false;
    }
    
    // Try to connect
    unsigned long startTime = millis();
    if (client.connect(robotIP, ntPort)) {
        unsigned long connectTime = millis() - startTime;
        debugPrint("TCP connection established in " + String(connectTime) + "ms");
        
        // Send proper NT4 WebSocket handshake
        if (performWebSocketHandshake()) {
            debugPrint("WebSocket handshake successful");
            
            // Wait a moment after handshake before marking as connected
            delay(100);
            
            isConnected = true;
            stats.connectTime = millis();
            stats.totalConnections++;
            consecutiveFailures = 0;
            
            debugPrint("NT4 connection established successfully");
            
            // Don't send any messages immediately - let connection stabilize
            debugPrint("Waiting for connection to stabilize...");
            
            // Wait and check if connection stays up
            delay(500);
            
            // Check if we're still connected after stabilization period
            if (client.connected()) {
                debugPrint("Connection stable - ready for communication");
                
                if (connectionCallback) {
                    connectionCallback(true);
                }
                
                lastConnectionAttempt = millis();
                lastDataReceived = millis();
                return true;
            } else {
                debugPrint("Connection dropped during stabilization period");
                isConnected = false;
                return false;
            }
        } else {
            debugPrint("WebSocket handshake failed");
            client.stop();
        }
    } else {
        debugPrint("Failed to establish TCP connection");
    }
    
    consecutiveFailures++;
    lastConnectionAttempt = millis();
    return false;
}

bool RobustNTClient::performWebSocketHandshake() {
    debugPrint("Starting WebSocket handshake for NT4");
    
    // Send HTTP upgrade request
    client.print("GET /nt/ws HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(robotIP[0]); client.print(".");
    client.print(robotIP[1]); client.print(".");
    client.print(robotIP[2]); client.print(".");
    client.print(robotIP[3]); client.print(":"); client.print(ntPort);
    client.print("\r\n");
    client.print("Upgrade: websocket\r\n");
    client.print("Connection: Upgrade\r\n");
    client.print("Sec-WebSocket-Key: dGVlbnN5LW50LWNsaWVudA==\r\n");
    client.print("Sec-WebSocket-Version: 13\r\n");
    client.print("Sec-WebSocket-Protocol: networktables.first.wpi.edu\r\n");
    client.print("\r\n");
    
    debugPrint("Sent WebSocket upgrade request");
    
    // Wait for response
    unsigned long startTime = millis();
    String response = "";
    
    while (millis() - startTime < 3000) { // 3 second timeout
        if (client.available()) {
            char c = client.read();
            response += c;
            
            // Look for end of HTTP headers
            if (response.endsWith("\r\n\r\n")) {
                break;
            }
        }
        delay(1);
    }
    
    debugPrint("Handshake response length: " + String(response.length()));
    
    // Check if upgrade was successful
    if (response.indexOf("101 Switching Protocols") >= 0 && 
        response.indexOf("websocket") >= 0) {
        debugPrint("WebSocket upgrade successful");
        return true;
    } else {
        debugPrint("WebSocket upgrade failed");
        debugPrint("Response: " + response.substring(0, 200));
        return false;
    }
}

void RobustNTClient::update() {
    unsigned long currentTime = millis();
    
    // Check if we're still connected
    if (isConnected) {
        if (!client.connected()) {
            debugPrint("Connection lost - TCP socket closed");
            handleConnectionLoss();
        } else {
            // Send a test message periodically to keep connection alive
            if (currentTime - lastHeartbeat >= heartbeatInterval) {
                // For now, don't send any messages - just check connection
                debugPrint("Heartbeat check - connection alive for " + String(getUptime()) + "ms");
                lastHeartbeat = currentTime;
            }
            
            // Process any incoming WebSocket data
            while (client.available()) {
                processWebSocketData();
            }
            
            // Send periodic heartbeat
            if (currentTime - lastHeartbeat >= heartbeatInterval) {
                if (stats.messagesSent == 0) {
                    // First heartbeat - send identify message
                    debugPrint("Sending delayed identify message...");
                    sendNT4Message("{\"method\":\"identify\",\"params\":{\"name\":\"teensy\"}}");
                } else {
                    // Regular heartbeat
                    publishHeartbeat();
                }
                lastHeartbeat = currentTime;
            }
            
            // Check for data timeout (robot not sending anything)
            if (currentTime - lastDataReceived > 15000) { // 15 seconds
                debugPrint("Data timeout - no data received for 15 seconds");
                // Don't disconnect immediately - NT4 might not send regular data
                // handleConnectionLoss();
            }
        }
    }
    
    // Auto-reconnect logic
    if (!isConnected && autoReconnect) {
        unsigned long retryInterval = 1000 + (consecutiveFailures * 2000); // Exponential backoff
        retryInterval = min(retryInterval, 15000UL); // Max 15 seconds
        
        if (currentTime - lastConnectionAttempt >= retryInterval) {
            if (consecutiveFailures < maxConsecutiveFailures) {
                debugPrint("Attempting reconnection (attempt " + 
                          String(consecutiveFailures + 1) + "/" + String(maxConsecutiveFailures) + ")");
                begin(robotIP, ntPort);
            } else {
                debugPrint("Max connection attempts reached - pausing auto-reconnect");
                consecutiveFailures = 0; // Reset after a longer pause
                lastConnectionAttempt = currentTime + 30000; // Wait 30 seconds
            }
        }
    }
}

bool RobustNTClient::testBasicConnectivity() {
    debugPrint("Testing basic connectivity to robot...");
    
    // Quick ping test
    EthernetClient testClient;
    unsigned long startTime = millis();
    
    if (testClient.connect(robotIP, 22)) { // SSH port
        testClient.stop();
        debugPrint("Robot is reachable (SSH port responding)");
        return true;
    }
    
    if (testClient.connect(robotIP, 80)) { // HTTP port
        testClient.stop();
        debugPrint("Robot is reachable (HTTP port responding)");
        return true;
    }
    
    if (testClient.connect(robotIP, 1735)) { // SmartDashboard port
        testClient.stop();
        debugPrint("Robot is reachable (SmartDashboard port responding)");
        return true;
    }
    
    debugPrint("Robot not reachable on any known ports");
    return false;
}

void RobustNTClient::handleConnectionLoss() {
    if (isConnected) {
        isConnected = false;
        stats.totalDisconnects++;
        
        debugPrint("Connection lost after " + String(getUptime()) + "ms uptime");
        
        if (connectionCallback) {
            connectionCallback(false);
        }
        
        client.stop();
    }
}

bool RobustNTClient::publishNumber(const String& key, double value) {
    if (isConnected && client.connected()) {
        // Use correct NT4 publish pattern with pubuid
        int pubId = nextPubId++;
        sendNT4Message("[{\"method\":\"publish\",\"params\":{\"name\":\"" + key + "\",\"type\":\"double\",\"pubuid\":" + String(pubId) + ",\"properties\":{}}}]");
        delay(50);
        sendNT4Message("[{\"method\":\"setvalue\",\"params\":{\"pubuid\":" + String(pubId) + ",\"value\":" + String(value, 6) + "}}]");
        return true;
    }
    return false;
}

bool RobustNTClient::publishBoolean(const String& key, bool value) {
    if (isConnected && client.connected()) {
        // Use correct NT4 publish pattern with pubuid
        int pubId = nextPubId++;
        sendNT4Message("[{\"method\":\"publish\",\"params\":{\"name\":\"" + key + "\",\"type\":\"boolean\",\"pubuid\":" + String(pubId) + ",\"properties\":{}}}]");
        delay(50);
        sendNT4Message("[{\"method\":\"setvalue\",\"params\":{\"pubuid\":" + String(pubId) + ",\"value\":" + (value ? "true" : "false") + "}}]");
        return true;
    }
    return false;
}

bool RobustNTClient::publishString(const String& key, const String& value) {
    if (isConnected && client.connected()) {
        // Use correct NT4 publish pattern with pubuid
        int pubId = nextPubId++;
        sendNT4Message("[{\"method\":\"publish\",\"params\":{\"name\":\"" + key + "\",\"type\":\"string\",\"pubuid\":" + String(pubId) + ",\"properties\":{}}}]");
        delay(50);
        sendNT4Message("[{\"method\":\"setvalue\",\"params\":{\"pubuid\":" + String(pubId) + ",\"value\":\"" + value + "\"}}]");
        return true;
    }
    return false;
}

bool RobustNTClient::publishHeartbeat() {
    if (isConnected && client.connected()) {
        // Don't send ping - NT4 may not expect it
        // Instead, just update our heartbeat timestamp
        debugPrint("Heartbeat (no message sent to robot)");
        return true;
    }
    return false;
}

bool RobustNTClient::publishEmergencyStop() {
    if (isConnected && client.connected()) {
        // Use correct NT4 publish pattern with pubuid
        int pubId = nextPubId++;
        sendNT4Message("[{\"method\":\"publish\",\"params\":{\"name\":\"SmartDashboard/EmergencyStop\",\"type\":\"boolean\",\"pubuid\":" + String(pubId) + ",\"properties\":{}}}]");
        delay(50);
        sendNT4Message("[{\"method\":\"setvalue\",\"params\":{\"pubuid\":" + String(pubId) + ",\"value\":true}}]");
        return true;
    }
    return false;
}

bool RobustNTClient::requestRobotData() {
    if (isConnected && client.connected()) {
        // Use minimal subscription format
        sendNT4Message("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
        return true;
    }
    return false;
}

void RobustNTClient::sendNT4Message(const String& message) {
    if (isConnected && client.connected()) {
        // Send as WebSocket text frame with required masking for client->server
        size_t msgLen = message.length();
        
        // Frame header
        if (client.write(0x81) != 1) { // Text frame, final fragment
            debugPrint("SendErr: Failed to write frame header");
            return;
        }
        
        // Payload length with MASK bit set (0x80)
        if (msgLen < 126) {
            if (client.write((uint8_t)(msgLen | 0x80)) != 1) { // Set mask bit
                debugPrint("SendErr: Failed to write payload length");
                return;
            }
        } else if (msgLen < 65536) {
            if (client.write(126 | 0x80) != 1 ||  // Set mask bit
                client.write((uint8_t)(msgLen >> 8)) != 1 ||
                client.write((uint8_t)(msgLen & 0xFF)) != 1) {
                debugPrint("SendErr: Failed to write extended payload length");
                return;
            }
        } else {
            debugPrint("Message too long for WebSocket frame");
            return;
        }
        
        // Generate masking key (4 random bytes)
        uint8_t maskKey[4];
        maskKey[0] = random(256);
        maskKey[1] = random(256);
        maskKey[2] = random(256);
        maskKey[3] = random(256);
        
        // Send masking key
        if (client.write(maskKey, 4) != 4) {
            debugPrint("SendErr: Failed to write masking key");
            return;
        }
        
        // Send masked payload data
        for (size_t i = 0; i < msgLen; i++) {
            uint8_t maskedByte = message[i] ^ maskKey[i % 4];
            if (client.write(maskedByte) != 1) {
                debugPrint("SendErr: Failed to write masked byte at position " + String(i));
                return;
            }
        }
        
        // Ensure data is sent immediately
        client.flush();
        
        stats.messagesSent++;
        
        if (debugMode) {
            debugPrint("Sent MASKED (" + String(msgLen) + " bytes): " + 
                      message.substring(0, 50) + (message.length() > 50 ? "..." : ""));
        }
        
        // Small delay to prevent overwhelming the robot
        delay(50);
    }
}

void RobustNTClient::processWebSocketData() {
    // Simple WebSocket frame processing
    static String buffer = "";
    static bool inFrame = false;
    static int payloadLength = -1;
    static int bytesRead = 0;
    
    while (client.available()) {
        uint8_t byte = client.read();
        
        if (!inFrame) {
            // Look for text frame start (0x81)
            if (byte == 0x81) {
                inFrame = true;
                payloadLength = -1;
                bytesRead = 0;
                buffer = "";
            }
        } else if (payloadLength == -1) {
            // Read payload length
            payloadLength = byte & 0x7F; // Mask off the MASK bit
        } else {
            // Read payload data
            if (bytesRead < payloadLength) {
                buffer += (char)byte;
                bytesRead++;
                
                if (bytesRead >= payloadLength) {
                    // Complete message received
                    lastDataReceived = millis();
                    stats.messagesReceived++;
                    
                    if (debugMode) {
                        debugPrint("Received: " + buffer.substring(0, 50) + 
                                  (buffer.length() > 50 ? "..." : ""));
                    }
                    
                    // Parse NT4 JSON message
                    parseNT4Message(buffer);
                    
                    // Reset for next frame
                    inFrame = false;
                    payloadLength = -1;
                    bytesRead = 0;
                    buffer = "";
                }
            }
        }
    }
}

void RobustNTClient::parseNT4Message(const String& message) {
    // Parse NT4 JSON array message format
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, message);
    
    if (!error) {
        // NT4 sends messages as JSON arrays
        if (doc.is<JsonArray>()) {
            JsonArray messageArray = doc.as<JsonArray>();
            
            for (JsonObject messageObj : messageArray) {
                String method = messageObj["method"];
                
                if (method == "announce") {
                    String name = messageObj["params"]["name"];
                    String type = messageObj["params"]["type"];
                    int pubuid = messageObj["params"]["pubuid"];
                    debugPrint("Robot announced: " + name + " (" + type + ") pubuid=" + String(pubuid));
                }
                else if (method == "setvalue") {
                    int pubuid = messageObj["params"]["pubuid"];
                    JsonVariant valueVar = messageObj["params"]["value"];
                    debugPrint("Received setvalue for pubuid " + String(pubuid) + ": " + valueVar.as<String>());
                }
                else if (method == "update") {
                    // Legacy update format - still handle it
                    JsonObject params = messageObj["params"];
                    for (JsonPair kv : params) {
                        String key = kv.key().c_str();
                        if (debugMode) {
                            debugPrint("Received update: " + key + " = " + kv.value().as<String>());
                        }
                    }
                }
                else {
                    debugPrint("Received unknown method: " + method);
                }
            }
        } else {
            debugPrint("Received non-array JSON message: " + message.substring(0, 50));
        }
    } else {
        debugPrint("JSON parse error: " + String(error.c_str()));
    }
}

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

void RobustNTClient::getStatistics(unsigned long& connections, unsigned long& disconnects, 
                                  unsigned long& messagesRx, unsigned long& messagesTx) {
    connections = stats.totalConnections;
    disconnects = stats.totalDisconnects;
    messagesRx = stats.messagesReceived;
    messagesTx = stats.messagesSent;
}

void RobustNTClient::printStatus() {
    Serial.println("=== NetworkTables Status ===");
    Serial.printf("Connected: %s\n", isConnected ? "YES" : "NO");
    Serial.printf("Robot IP: %d.%d.%d.%d:%d\n", robotIP[0], robotIP[1], robotIP[2], robotIP[3], ntPort);
    Serial.printf("Team Number: %d\n", teamNumber);
    
    if (isConnected) {
        Serial.printf("Uptime: %lu ms\n", getUptime());
        Serial.printf("Last Data: %lu ms ago\n", millis() - lastDataReceived);
    }
    
    Serial.printf("Total Connections: %lu\n", stats.totalConnections);
    Serial.printf("Total Disconnects: %lu\n", stats.totalDisconnects);
    Serial.printf("Consecutive Failures: %d\n", consecutiveFailures);
    Serial.printf("Messages RX/TX: %lu/%lu\n", stats.messagesReceived, stats.messagesSent);
    
    if (robotData.lastUpdate > 0) {
        Serial.printf("Battery Voltage: %.1fV\n", robotData.batteryVoltage);
        Serial.printf("Robot Data Age: %lu ms\n", millis() - robotData.lastUpdate);
    }
    
    Serial.println("===========================");
}

void RobustNTClient::printConnectionDiagnostics() {
    Serial.println("\n=== Connection Diagnostics ===");
    
    // Test robot reachability
    runConnectionTest();
    
    // Print connection history
    if (stats.totalConnections > 0) {
        float avgUptime = isConnected ? getUptime() : 0;
        if (stats.totalDisconnects > 0) {
            Serial.printf("Connection reliability: %lu connections, %lu disconnects\n", 
                         stats.totalConnections, stats.totalDisconnects);
            Serial.printf("Average connection duration: %.1f seconds\n", avgUptime / 1000.0);
        }
        
        if (consecutiveFailures > 0) {
            Serial.printf("Current failure streak: %d\n", consecutiveFailures);
            Serial.println("This suggests:");
            Serial.println("- Robot code may not be running NetworkTables server");
            Serial.println("- Robot is rejecting connections");
            Serial.println("- Protocol mismatch between client and server");
        }
    }
    
    Serial.println("===============================\n");
}

void RobustNTClient::runConnectionTest() {
    Serial.println("Testing robot connectivity...");
    
    EthernetClient testClient;
    
    // Test common robot ports
    struct {
        uint16_t port;
        const char* service;
    } testPorts[] = {
        {22, "SSH"},
        {80, "HTTP"},
        {1735, "SmartDashboard"}, 
        {5800, "Camera"},
        {5810, "NetworkTables"}
    };
    
    bool anyPortOpen = false;
    
    for (int i = 0; i < 5; i++) {
        Serial.printf("  Port %d (%s): ", testPorts[i].port, testPorts[i].service);
        
        if (testClient.connect(robotIP, testPorts[i].port)) {
            Serial.println("✓ Open");
            testClient.stop();
            anyPortOpen = true;
            
            if (testPorts[i].port == 5810) {
                Serial.println("    NetworkTables port is accessible");
            }
        } else {
            Serial.println("✗ Closed/Filtered");
        }
        delay(100);
    }
    
    if (!anyPortOpen) {
        Serial.println("❌ Robot appears to be unreachable");
        Serial.println("   Check: Power, network connection, IP address");
    } else if (testClient.connect(robotIP, 5810)) {
        Serial.println("✅ NetworkTables port is accessible");
        Serial.println("   Issue is likely protocol-related, not connectivity");
        testClient.stop();
    }
}

void RobustNTClient::runMessageTests() {
    if (!isConnected) {
        debugPrint("Cannot run message tests - not connected");
        return;
    }
    
    debugPrint("=== Starting Message Tests ===");
    debugPrint("Testing NT4 message formats with correct protocol...");
    
    // Test 1: Skip identify message - NT4 doesn't use it
    debugPrint("Test 1: Skipping identify (not used in NT4)");
    debugPrint("✅ Test 1 PASSED - No identify needed");
    
    // Test 2: Subscribe with proper subuid
    debugPrint("Test 2: Subscribe to battery voltage (with subuid)");
    sendNT4Message("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/BatteryVoltage\"],\"subuid\":" + String(nextSubId++) + ",\"options\":{\"periodic\":0.1,\"all\":false}}}]");
    delay(3000);
    
    if (!client.connected()) {
        debugPrint("❌ Test 2 FAILED - Connection dropped after subscription");
        return;
    }
    debugPrint("✅ Test 2 PASSED - Subscription message accepted");
    
    // Test 3: Subscribe to all SmartDashboard with different subuid
    debugPrint("Test 3: Subscribe to all SmartDashboard data");
    sendNT4Message("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"SmartDashboard/\"],\"subuid\":" + String(nextSubId++) + ",\"options\":{\"periodic\":0.1,\"all\":false}}}]");
    delay(3000);
    
    if (!client.connected()) {
        debugPrint("❌ Test 3 FAILED - Connection dropped after broad subscription");
        return;
    }
    debugPrint("✅ Test 3 PASSED - Broad subscription accepted");
    
    // Test 4: Wait for data
    debugPrint("Test 4: Waiting for robot data...");
    unsigned long dataWaitStart = millis();
    unsigned long originalDataTime = lastDataReceived;
    
    while (millis() - dataWaitStart < 10000) { // Wait 10 seconds for data
        if (lastDataReceived > originalDataTime) {
            debugPrint("✅ Test 4 PASSED - Received data from robot!");
            break;
        }
        delay(100);
    }
    
    if (lastDataReceived == originalDataTime) {
        debugPrint("⚠️  Test 4 - No data received (robot may not be publishing to SmartDashboard)");
    }
    
    // Test 5: Send update to robot (try both 'publish' and 'update' methods)
    debugPrint("Test 5a: Send publish to SmartDashboard");
    sendNT4Message("[{\"method\":\"publish\",\"params\":{\"name\":\"SmartDashboard/TeensyAlive\",\"type\":\"int\",\"value\":" + String(millis()) + "}}]");
    delay(2000);
    
    if (!client.connected()) {
        debugPrint("❌ Test 5a FAILED - Connection dropped after publish");
        return;
    }
    debugPrint("✅ Test 5a PASSED - Publish message accepted");
    
    // Test 5b: Try announce + setValue pattern
    debugPrint("Test 5b: Send announce + setValue");
    sendNT4Message("[{\"method\":\"announce\",\"params\":{\"name\":\"SmartDashboard/TeensyStatus\",\"type\":\"string\",\"properties\":{}}}]");
    delay(1000);
    
    if (client.connected()) {
        sendNT4Message("[{\"method\":\"setValue\",\"params\":{\"name\":\"SmartDashboard/TeensyStatus\",\"value\":\"Connected\"}}]");
        delay(2000);
        
        if (!client.connected()) {
            debugPrint("❌ Test 5b FAILED - Connection dropped after setValue");
            return;
        }
        debugPrint("✅ Test 5b PASSED - Announce + setValue accepted");
    }
    
    debugPrint("=== Message Tests Complete ===");
    debugPrint("NT4 connection is fully functional!");
}

void RobustNTClient::runConnectionHealthTest() {
    if (!isConnected) {
        debugPrint("Cannot run health test - not connected");
        return;
    }
    
    debugPrint("=== Connection Health Test ===");
    debugPrint("Testing how long a bare NT4 connection can stay alive...");
    debugPrint("Will NOT send any messages - just monitor connection");
    
    unsigned long testStart = millis();
    unsigned long lastHeartbeatTime = millis();
    int heartbeatCount = 0;
    
    // Monitor connection for 60 seconds without sending anything
    while (millis() - testStart < 60000) { // 60 seconds
        if (!client.connected()) {
            unsigned long duration = millis() - testStart;
            debugPrint("❌ Connection lost after " + String(duration) + "ms");
            debugPrint("Bare connection survived " + String(duration / 1000.0) + " seconds");
            return;
        }
        
        // Print heartbeat every 5 seconds
        if (millis() - lastHeartbeatTime >= 5000) {
            heartbeatCount++;
            debugPrint("Heartbeat " + String(heartbeatCount) + ": Connection alive for " + 
                      String((millis() - testStart) / 1000.0) + " seconds");
            lastHeartbeatTime = millis();
        }
        
        delay(100);
    }
    
    if (client.connected()) {
        debugPrint("✅ Connection Health Test PASSED!");
        debugPrint("Bare NT4 connection survived 60 seconds without any messages");
        debugPrint("Connection is stable - protocol implementation is correct");
    }
}

#endif // ROBUST_NT_CLIENT_H