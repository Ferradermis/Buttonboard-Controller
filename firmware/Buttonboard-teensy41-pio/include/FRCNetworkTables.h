/*
 * FRC NetworkTables Client for Teensy 4.1
 * 
 * This class implements a simplified NetworkTables v4 client for FRC robots.
 * Designed to run on Teensy 4.1 with native Ethernet to monitor robot status
 * and display information on a custom button board controller.
 * 
 * Features:
 * - Connects to robot NetworkTables server
 * - Subscribes to robot status topics
 * - Handles april tag detection status
 * - Tracks command completion states
 * - Automatic reconnection on connection loss
 * - LED status indicators
 * 
 * Hardware Requirements:
 * - Teensy 4.1 with Ethernet
 * - Status LEDs (optional)
 * - Button board interface
 * 
 * Author: Custom FRC Integration
 */

#ifndef FRC_NETWORKTABLES_H
#define FRC_NETWORKTABLES_H

#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <ArduinoJson.h>

class FRCNetworkTables {

public:
    // Robot connection status
    enum ConnectionStatus {
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        RECONNECTING = 3
    };
    
    // Robot states we care about
    struct RobotStatus {
        bool enabled = false;
        bool autonomous = false;
        bool teleop = false;
        bool test = false;
        bool dsConnected = false;
        float batteryVoltage = 0.0;
        
        // April tag detection
        bool aprilTagDetected = false;
        int aprilTagId = -1;
        float aprilTagDistance = 0.0;
        
        // Command status
        bool intakeComplete = false;
        bool shooterReady = false;
        bool climberReady = false;
        bool autoComplete = false;
        
        // Custom status indicators
        bool alliance = false; // false = red, true = blue
        int matchTime = 0;
    };
    
    // Constructor
    FRCNetworkTables(uint16_t robotNumber = 1234);
    
    // Initialization
    bool begin(uint8_t* mac = nullptr);
    bool connect();
    void disconnect();
    
    // Main update loop - call frequently
    void update();
    
    // Status getters
    ConnectionStatus getConnectionStatus() { return _connectionStatus; }
    RobotStatus getRobotStatus() { return _robotStatus; }
    bool isConnected() { return _connectionStatus == CONNECTED; }
    unsigned long getLastUpdate() { return _lastUpdate; }
    
    // Topic subscription management
    void subscribeToTopic(const String& topic);
    void unsubscribeFromTopic(const String& topic);
    
    // Value getters for specific topics
    bool getBoolValue(const String& topic, bool defaultValue = false);
    double getDoubleValue(const String& topic, double defaultValue = 0.0);
    String getStringValue(const String& topic, const String& defaultValue = "");
    
    // Configuration
    void setUpdateInterval(unsigned long intervalMs) { _updateInterval = intervalMs; }
    void setConnectionTimeout(unsigned long timeoutMs) { _connectionTimeout = timeoutMs; }
    void setRobotNumber(uint16_t robotNumber);
    
    // Debug and diagnostics
    void printStatus();
    void enableDebug(bool enable = true) { _debug = enable; }
    
    //utility
    String formatIPAddress(IPAddress ip);

private:
    // Network configuration
    uint16_t _robotNumber;
    IPAddress _robotIP;
    uint16_t _ntPort;
    EthernetClient _client;
    
    // Connection management
    ConnectionStatus _connectionStatus;
    unsigned long _lastConnectionAttempt;
    unsigned long _connectionTimeout;
    unsigned long _reconnectInterval;
    
    // Data management
    RobotStatus _robotStatus;
    unsigned long _lastUpdate;
    unsigned long _updateInterval;
    
    // Topic management
    struct TopicInfo {
        String name;
        String type;
        int id;
        bool subscribed;
    };
    
    static const int MAX_TOPICS = 50;
    TopicInfo _topics[MAX_TOPICS];
    int _topicCount;
    
    // Message handling
    struct Message {
        String type;
        JsonDocument data;
    };
    
    // Internal state
    bool _debug;
    int _clientId;
    bool _handshakeComplete;
    
    // Core NetworkTables protocol methods
    bool performHandshake();
    bool sendMessage(const String& type, JsonDocument& data);
    bool receiveMessage(Message& msg);
    void processMessage(const Message& msg);
    
    // Topic management
    int findTopicIndex(const String& topic);
    int addTopic(const String& topic, const String& type);
    void updateTopicValue(int topicId, JsonVariantConst value);
    
    // Robot status parsing
    void updateRobotStatus();
    void parseDriverStationData(JsonDocument& data);
    void parseRobotData(JsonDocument& data);
    void parseVisionData(JsonDocument& data);
    
    // Connection helpers
    IPAddress calculateRobotIP(uint16_t robotNumber);
    bool attemptConnection();
    void handleDisconnection();
    
    // Utility methods
    void debugPrint(const String& msg);
    
};


// Implementation
FRCNetworkTables::FRCNetworkTables(uint16_t robotNumber) {
    _robotNumber = robotNumber;
    _robotIP = calculateRobotIP(robotNumber);
    _ntPort = 5810; // Standard NetworkTables port
    
    _connectionStatus = DISCONNECTED;
    _lastConnectionAttempt = 0;
    _connectionTimeout = 5000; // 5 second timeout
    _reconnectInterval = 2000; // Try reconnecting every 2 seconds
    
    _lastUpdate = 0;
    _updateInterval = 100; // Update every 100ms
    
    _topicCount = 0;
    _debug = false;
    _clientId = random(1000, 9999);
    _handshakeComplete = false;
    
    // Initialize robot status with default values
    _robotStatus = RobotStatus(); // Use default constructor
}

bool FRCNetworkTables::begin(uint8_t* mac) {
    // Generate MAC if not provided
    uint8_t defaultMac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
    if (mac == nullptr) {
        mac = defaultMac;
    }
    
    // Initialize Ethernet
    if (Ethernet.begin(mac) == 0) {
        debugPrint("Failed to configure Ethernet using DHCP");
        
        // Try with static IP as fallback
        IPAddress ip(10, _robotNumber / 100, (_robotNumber % 100), 100);
        IPAddress dns(8, 8, 8, 8);
        IPAddress gateway(10, _robotNumber / 100, (_robotNumber % 100), 1);
        IPAddress subnet(255, 255, 255, 0);
        
        Ethernet.begin(mac, ip, dns, gateway, subnet);
        debugPrint("Using static IP: " + formatIPAddress(Ethernet.localIP()));
    } else {
        debugPrint("DHCP IP: " + formatIPAddress(Ethernet.localIP()));
    }
    
    // Subscribe to essential robot topics
    subscribeToTopic("/FMSInfo/IsRedAlliance");
    subscribeToTopic("/FMSInfo/MatchTime");
    subscribeToTopic("/DriverStation/Enabled");
    subscribeToTopic("/DriverStation/Autonomous");
    subscribeToTopic("/DriverStation/Test");
    subscribeToTopic("/PowerDistribution/Voltage");
    
    // Custom robot topics - adjust these for your robot
    subscribeToTopic("/Robot/IntakeComplete");
    subscribeToTopic("/Robot/ShooterReady");
    subscribeToTopic("/Robot/ClimberReady");
    subscribeToTopic("/Robot/AutoComplete");
    
    // Vision/AprilTag topics
    subscribeToTopic("/Vision/AprilTagDetected");
    subscribeToTopic("/Vision/AprilTagID");
    subscribeToTopic("/Vision/AprilTagDistance");
    
    return true;
}

bool FRCNetworkTables::connect() {
    if (_connectionStatus == CONNECTING || _connectionStatus == CONNECTED) {
        return _connectionStatus == CONNECTED;
    }
    
    unsigned long now = millis();
    if (now - _lastConnectionAttempt < _reconnectInterval) {
        return false;
    }
    
    _lastConnectionAttempt = now;
    _connectionStatus = CONNECTING;
    
    debugPrint("Attempting to connect to robot at " + formatIPAddress(_robotIP));
    
    if (attemptConnection()) {
        _connectionStatus = CONNECTED;
        debugPrint("Connected to robot NetworkTables");
        return true;
    } else {
        _connectionStatus = DISCONNECTED;
        debugPrint("Failed to connect to robot");
        return false;
    }
}

void FRCNetworkTables::update() {
    unsigned long now = millis();
    
    // Handle connection state
    switch (_connectionStatus) {
        case DISCONNECTED:
            if (now - _lastConnectionAttempt > _reconnectInterval) {
                connect();
            }
            break;
            
        case CONNECTING:
            // Timeout check handled in connect()
            break;
            
        case CONNECTED:
            {
                if (!_client.connected()) {
                    handleDisconnection();
                    return;
                }
                
                // Process incoming messages
                Message msg;
                while (receiveMessage(msg)) {
                    processMessage(msg);
                }
                
                // Send periodic heartbeat
                if (now - _lastUpdate > _updateInterval) {
                    updateRobotStatus();
                    _lastUpdate = now;
                }
            }
            break;
            
        case RECONNECTING:
            _connectionStatus = DISCONNECTED;
            break;
    }
}

void FRCNetworkTables::subscribeToTopic(const String& topic) {
    if (findTopicIndex(topic) != -1) {
        return; // Already subscribed
    }
    
    if (_topicCount >= MAX_TOPICS) {
        debugPrint("Topic limit reached, cannot subscribe to: " + topic);
        return;
    }
    
    int index = addTopic(topic, "unknown");
    if (index != -1 && _connectionStatus == CONNECTED) {
        // Send subscription message
        JsonDocument doc;
        doc["topic"] = topic;
        doc["subscribe"] = true;
        
        sendMessage("subscribe", doc);
        _topics[index].subscribed = true;
        
        debugPrint("Subscribed to topic: " + topic);
    }
}

bool FRCNetworkTables::getBoolValue(const String& topic, bool defaultValue) {
    int index = findTopicIndex(topic);
    if (index == -1) return defaultValue;
    
    // In a real implementation, you'd store the values
    // For now, we'll update the robot status directly
    return defaultValue;
}

double FRCNetworkTables::getDoubleValue(const String& topic, double defaultValue) {
    int index = findTopicIndex(topic);
    if (index == -1) return defaultValue;
    return defaultValue;
}

void FRCNetworkTables::updateRobotStatus() {
    // This would be called to parse and update robot status from received data
    // The actual values would come from processMessage()
    
    // Example of how you might update status based on received NT data:
    _robotStatus.enabled = getBoolValue("/DriverStation/Enabled", false);
    _robotStatus.autonomous = getBoolValue("/DriverStation/Autonomous", false);
    _robotStatus.teleop = _robotStatus.enabled && !_robotStatus.autonomous;
    _robotStatus.batteryVoltage = getDoubleValue("/PowerDistribution/Voltage", 0.0);
    
    // Update custom robot status
    _robotStatus.intakeComplete = getBoolValue("/Robot/IntakeComplete", false);
    _robotStatus.shooterReady = getBoolValue("/Robot/ShooterReady", false);
    _robotStatus.climberReady = getBoolValue("/Robot/ClimberReady", false);
    _robotStatus.autoComplete = getBoolValue("/Robot/AutoComplete", false);
    
    // Update vision data
    _robotStatus.aprilTagDetected = getBoolValue("/Vision/AprilTagDetected", false);
    _robotStatus.aprilTagId = (int)getDoubleValue("/Vision/AprilTagID", -1);
    _robotStatus.aprilTagDistance = getDoubleValue("/Vision/AprilTagDistance", 0.0);
    
    // Alliance and match info
    _robotStatus.alliance = !getBoolValue("/FMSInfo/IsRedAlliance", true); // true = blue
    _robotStatus.matchTime = (int)getDoubleValue("/FMSInfo/MatchTime", 0);
}

IPAddress FRCNetworkTables::calculateRobotIP(uint16_t robotNumber) {
    // Standard FRC robot IP calculation: 10.TE.AM.2
    // where TEAM is the 4-digit team number
    uint8_t te = robotNumber / 100;
    uint8_t am = robotNumber % 100;
    return IPAddress(10, te, am, 2);
}

bool FRCNetworkTables::attemptConnection() {
    if (_client.connect(_robotIP, _ntPort)) {
        return performHandshake();
    }
    return false;
}

bool FRCNetworkTables::performHandshake() {
    // Simplified NetworkTables handshake
    // In a full implementation, this would follow the NT4 protocol
    
    JsonDocument handshake;
    handshake["type"] = "handshake";
    handshake["clientId"] = _clientId;
    handshake["version"] = "4.0";
    
    if (sendMessage("handshake", handshake)) {
        _handshakeComplete = true;
        return true;
    }
    return false;
}

bool FRCNetworkTables::sendMessage(const String& type, JsonDocument& data) {
    if (!_client.connected()) return false;
    
    String message;
    serializeJson(data, message);
    
    _client.println(type + ":" + message);
    return true;
}

bool FRCNetworkTables::receiveMessage(Message& msg) {
    if (!_client.available()) return false;
    
    String line = _client.readStringUntil('\n');
    line.trim();
    
    int colonPos = line.indexOf(':');
    if (colonPos == -1) return false;
    
    msg.type = line.substring(0, colonPos);
    String jsonStr = line.substring(colonPos + 1);
    
    DeserializationError error = deserializeJson(msg.data, jsonStr);
    if (error) {
        debugPrint("JSON parse error: " + String(error.c_str()));
        return false;
    }
    
    return true;
}

void FRCNetworkTables::processMessage(const Message& msg) {
    if (msg.type == "update") {
        // Handle topic value updates
        if (msg.data["topic"].is<const char*>() && !msg.data["value"].isNull()) {
            String topic = msg.data["topic"].as<const char*>();
            
            int topicIndex = findTopicIndex(topic);
            if (topicIndex != -1) {
                updateTopicValue(topicIndex, msg.data["value"]);
            }
        }
    } else if (msg.type == "announce") {
        // Handle new topic announcements
        if (msg.data["name"].is<const char*>() && msg.data["type"].is<const char*>()) {
            String name = msg.data["name"].as<const char*>();
            String type = msg.data["type"].as<const char*>();
            addTopic(name, type);
        }
    }
}

int FRCNetworkTables::findTopicIndex(const String& topic) {
    for (int i = 0; i < _topicCount; i++) {
        if (_topics[i].name == topic) {
            return i;
        }
    }
    return -1;
}

int FRCNetworkTables::addTopic(const String& topic, const String& type) {
    if (_topicCount >= MAX_TOPICS) return -1;
    
    _topics[_topicCount].name = topic;
    _topics[_topicCount].type = type;
    _topics[_topicCount].id = _topicCount;
    _topics[_topicCount].subscribed = false;
    
    return _topicCount++;
}

void FRCNetworkTables::updateTopicValue(int topicId, JsonVariantConst value) {
    if (topicId < 0 || topicId >= _topicCount) return;
    
    String topic = _topics[topicId].name;
    
    // Update robot status based on topic
    if (topic == "/DriverStation/Enabled") {
        _robotStatus.enabled = value.as<bool>();
    } else if (topic == "/DriverStation/Autonomous") {
        _robotStatus.autonomous = value.as<bool>();
    } else if (topic == "/PowerDistribution/Voltage") {
        _robotStatus.batteryVoltage = value.as<float>();
    } else if (topic == "/Vision/AprilTagDetected") {
        _robotStatus.aprilTagDetected = value.as<bool>();
    } else if (topic == "/Vision/AprilTagID") {
        _robotStatus.aprilTagId = value.as<int>();
    } else if (topic == "/Vision/AprilTagDistance") {
        _robotStatus.aprilTagDistance = value.as<float>();
    } else if (topic == "/Robot/IntakeComplete") {
        _robotStatus.intakeComplete = value.as<bool>();
    } else if (topic == "/Robot/ShooterReady") {
        _robotStatus.shooterReady = value.as<bool>();
    } else if (topic == "/Robot/ClimberReady") {
        _robotStatus.climberReady = value.as<bool>();
    } else if (topic == "/Robot/AutoComplete") {
        _robotStatus.autoComplete = value.as<bool>();
    }
}

void FRCNetworkTables::handleDisconnection() {
    debugPrint("Robot connection lost");
    _connectionStatus = DISCONNECTED;
    _handshakeComplete = false;
    _client.stop();
    
    // Reset robot status on disconnect
    _robotStatus = RobotStatus(); // Use default constructor instead of memset
}

void FRCNetworkTables::setRobotNumber(uint16_t robotNumber) {
    _robotNumber = robotNumber;
    _robotIP = calculateRobotIP(robotNumber);
}

void FRCNetworkTables::printStatus() {
    Serial.println("=== FRC NetworkTables Status ===");
    Serial.println("Robot: " + String(_robotNumber));
    Serial.println("Robot IP: " + formatIPAddress(_robotIP));
    Serial.println("Local IP: " + formatIPAddress(Ethernet.localIP()));
    
    String statusStr = "UNKNOWN";
    switch (_connectionStatus) {
        case DISCONNECTED: statusStr = "DISCONNECTED"; break;
        case CONNECTING: statusStr = "CONNECTING"; break;
        case CONNECTED: statusStr = "CONNECTED"; break;
        case RECONNECTING: statusStr = "RECONNECTING"; break;
    }
    Serial.println("Connection: " + statusStr);
    
    Serial.println("Subscribed topics: " + String(_topicCount));
    Serial.println("Last update: " + String(millis() - _lastUpdate) + "ms ago");
    
    if (_connectionStatus == CONNECTED) {
        Serial.println("\n--- Robot Status ---");
        Serial.println("Enabled: " + String(_robotStatus.enabled ? "YES" : "NO"));
        Serial.println("Mode: " + String(_robotStatus.autonomous ? "AUTO" : "TELEOP"));
        Serial.println("Battery: " + String(_robotStatus.batteryVoltage, 1) + "V");
        Serial.println("AprilTag: " + String(_robotStatus.aprilTagDetected ? "DETECTED" : "NONE"));
        if (_robotStatus.aprilTagDetected) {
            Serial.println("  ID: " + String(_robotStatus.aprilTagId));
            Serial.println("  Distance: " + String(_robotStatus.aprilTagDistance, 1));
        }
        Serial.println("Intake: " + String(_robotStatus.intakeComplete ? "COMPLETE" : "BUSY"));
        Serial.println("Shooter: " + String(_robotStatus.shooterReady ? "READY" : "NOT READY"));
    }
    Serial.println("===============================");
}

void FRCNetworkTables::debugPrint(const String& msg) {
    if (_debug) {
        Serial.println("[NT] " + msg);
    }
}

String FRCNetworkTables::formatIPAddress(IPAddress ip) {
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

#endif // FRC_NETWORKTABLES_H