/*
 * NetworkTables Subscriber Implementation for Teensy 4.1
 * Provides reliable NT4 client functionality for FRC control boards
 */

#ifndef NETWORKTABLES_SUBSCRIBER_H
#define NETWORKTABLES_SUBSCRIBER_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <map>
#include <functional>

// Forward declarations
class NetworkTablesSubscriber;

// Callback types
typedef std::function<void(const String& key, double value)> NumberCallback;
typedef std::function<void(const String& key, bool value)> BooleanCallback;
typedef std::function<void(const String& key, const String& value)> StringCallback;
typedef std::function<void(bool connected)> ConnectionCallback;

// Subscription entry structure
struct Subscription {
    String key;
    String dataType;
    std::function<void(const String&)> callback;
    bool active;
    uint32_t lastUpdate;
};

class NetworkTablesSubscriber {
private:
    // Network configuration
    int teamNumber;
    IPAddress robotIP;
    uint16_t ntPort;
    
    // WebSocket client for NT4 protocol
    WebSocketsClient webSocket;
    bool isConnected;
    bool autoReconnect;
    
    // Connection management
    unsigned long lastConnectionAttempt;
    unsigned long connectionTimeout;
    unsigned long heartbeatInterval;
    unsigned long lastHeartbeat;
    
    // Subscriptions management
    std::map<String, Subscription> subscriptions;
    uint32_t nextSubscriptionId;
    
    // Callbacks
    ConnectionCallback connectionCallback;
    
    // Statistics
    struct {
        unsigned long connectTime;
        unsigned long totalDisconnects;
        unsigned long messagesReceived;
        unsigned long messagesSent;
        unsigned long lastRTT;
    } stats;
    
    // Message handling
    void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void processNTMessage(const String& message);
    void sendSubscription(const String& key, const String& dataType);
    void sendUnsubscription(const String& key);
    void sendHeartbeat();
    
    // Utility functions
    String createSubscribeMessage(const String& key, const String& dataType);
    String createUnsubscribeMessage(const String& key);
    IPAddress calculateRobotIP(int teamNumber);
    void updateStatistics();
    String ipToString(IPAddress ip);

public:
    NetworkTablesSubscriber(int teamNumber = 0);
    ~NetworkTablesSubscriber();
    
    // Connection management
    bool begin();
    bool begin(IPAddress customIP, uint16_t port = 5810);
    void setTeamNumber(int team);
    void setAutoReconnect(bool enable);
    void setConnectionCallback(ConnectionCallback callback);
    
    // Main loop - call this frequently
    void update();
    
    // Connection status
    bool connected() const { return isConnected; }
    unsigned long getUptime() const;
    void getStatistics(unsigned long& uptime, unsigned long& disconnects, 
                      unsigned long& messagesRx, unsigned long& messagesTx);
    
    // Subscription management
    bool subscribeNumber(const String& key, NumberCallback callback);
    bool subscribeBoolean(const String& key, BooleanCallback callback);
    bool subscribeString(const String& key, StringCallback callback);
    bool unsubscribe(const String& key);
    void unsubscribeAll();
    
    // Publishing (optional - for sending data to robot)
    bool publishNumber(const String& key, double value);
    bool publishBoolean(const String& key, bool value);
    bool publishString(const String& key, const String& value);
    
    // Utility
    void printStatus();
    void printSubscriptions();
};

// Implementation

NetworkTablesSubscriber::NetworkTablesSubscriber(int teamNumber) :
    teamNumber(teamNumber),
    ntPort(5810),
    isConnected(false),
    autoReconnect(true),
    lastConnectionAttempt(0),
    connectionTimeout(10000),
    heartbeatInterval(5000),
    lastHeartbeat(0),
    nextSubscriptionId(1),
    connectionCallback(nullptr)
{
    robotIP = calculateRobotIP(teamNumber);
    memset(&stats, 0, sizeof(stats));
    
    // Setup WebSocket event handler
    webSocket.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->handleWebSocketEvent(type, payload, length);
    });
}

NetworkTablesSubscriber::~NetworkTablesSubscriber() {
    webSocket.disconnect();
}

bool NetworkTablesSubscriber::begin() {
    if (teamNumber == 0) {
        Serial.println("NT: Error - Team number not set");
        return false;
    }
    
    return begin(robotIP, ntPort);
}

bool NetworkTablesSubscriber::begin(IPAddress customIP, uint16_t port) {
    robotIP = customIP;
    ntPort = port;
    
    Serial.printf("NT: Connecting to %s:%d\n", ipToString(robotIP).c_str(), ntPort);
    
    // Configure WebSocket
    String ipStr = ipToString(robotIP);
    webSocket.begin(ipStr.c_str(), ntPort, "/nt/ws");
    webSocket.setReconnectInterval(5000);
    
    lastConnectionAttempt = millis();
    return true;
}

void NetworkTablesSubscriber::setTeamNumber(int team) {
    teamNumber = team;
    robotIP = calculateRobotIP(team);
}

void NetworkTablesSubscriber::setAutoReconnect(bool enable) {
    autoReconnect = enable;
}

void NetworkTablesSubscriber::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback = callback;
}

void NetworkTablesSubscriber::update() {
    webSocket.loop();
    
    unsigned long currentTime = millis();
    
    // Send periodic heartbeat when connected
    if (isConnected && (currentTime - lastHeartbeat >= heartbeatInterval)) {
        sendHeartbeat();
        lastHeartbeat = currentTime;
    }
    
    // Auto-reconnect if enabled and disconnected
    if (!isConnected && autoReconnect && 
        (currentTime - lastConnectionAttempt >= 5000)) {
        Serial.println("NT: Attempting reconnection...");
        lastConnectionAttempt = currentTime;
        webSocket.disconnect();
        webSocket.begin(ipStr, ntPort, "/nt/ws");
    }
}

bool NetworkTablesSubscriber::subscribeNumber(const String& key, NumberCallback callback) {
    Subscription sub;
    sub.key = key;
    sub.dataType = "double";
    sub.callback = [key, callback](const String& valueStr) {
        callback(key, valueStr.toDouble());
    };
    sub.active = true;
    sub.lastUpdate = millis();
    
    subscriptions[key] = sub;
    
    if (isConnected) {
        sendSubscription(key, "double");
    }
    
    Serial.printf("NT: Subscribed to number: %s\n", key.c_str());
    return true;
}

bool NetworkTablesSubscriber::subscribeBoolean(const String& key, BooleanCallback callback) {
    Subscription sub;
    sub.key = key;
    sub.dataType = "boolean";
    sub.callback = [key, callback](const String& valueStr) {
        bool value = (valueStr == "true" || valueStr == "1");
        callback(key, value);
    };
    sub.active = true;
    sub.lastUpdate = millis();
    
    subscriptions[key] = sub;
    
    if (isConnected) {
        sendSubscription(key, "boolean");
    }
    
    Serial.printf("NT: Subscribed to boolean: %s\n", key.c_str());
    return true;
}

bool NetworkTablesSubscriber::subscribeString(const String& key, StringCallback callback) {
    Subscription sub;
    sub.key = key;
    sub.dataType = "string";
    sub.callback = [key, callback](const String& valueStr) {
        callback(key, valueStr);
    };
    sub.active = true;
    sub.lastUpdate = millis();
    
    subscriptions[key] = sub;
    
    if (isConnected) {
        sendSubscription(key, "string");
    }
    
    Serial.printf("NT: Subscribed to string: %s\n", key.c_str());
    return true;
}

bool NetworkTablesSubscriber::unsubscribe(const String& key) {
    auto it = subscriptions.find(key);
    if (it != subscriptions.end()) {
        if (isConnected) {
            sendUnsubscription(key);
        }
        subscriptions.erase(it);
        Serial.printf("NT: Unsubscribed from: %s\n", key.c_str());
        return true;
    }
    return false;
}

void NetworkTablesSubscriber::unsubscribeAll() {
    for (auto& pair : subscriptions) {
        if (isConnected) {
            sendUnsubscription(pair.first);
        }
    }
    subscriptions.clear();
    Serial.println("NT: Unsubscribed from all keys");
}

bool NetworkTablesSubscriber::publishNumber(const String& key, double value) {
    if (!isConnected) return false;
    
    DynamicJsonDocument doc(256);
    doc["method"] = "announce";
    doc["params"]["name"] = key;
    doc["params"]["type"] = "double";
    doc["params"]["properties"] = JsonObject();
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    
    // Send the value
    doc.clear();
    doc["method"] = "update";
    doc["params"][key] = value;
    
    message = "";
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    
    stats.messagesSent += 2;
    return true;
}

bool NetworkTablesSubscriber::publishBoolean(const String& key, bool value) {
    if (!isConnected) return false;
    
    DynamicJsonDocument doc(256);
    doc["method"] = "announce";
    doc["params"]["name"] = key;
    doc["params"]["type"] = "boolean";
    doc["params"]["properties"] = JsonObject();
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    
    // Send the value
    doc.clear();
    doc["method"] = "update";
    doc["params"][key] = value;
    
    message = "";
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    
    stats.messagesSent += 2;
    return true;
}

bool NetworkTablesSubscriber::publishString(const String& key, const String& value) {
    if (!isConnected) return false;
    
    DynamicJsonDocument doc(256);
    doc["method"] = "announce";
    doc["params"]["name"] = key;
    doc["params"]["type"] = "string";
    doc["params"]["properties"] = JsonObject();
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    
    // Send the value
    doc.clear();
    doc["method"] = "update";
    doc["params"][key] = value;
    
    message = "";
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    
    stats.messagesSent += 2;
    return true;
}

void NetworkTablesSubscriber::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            if (isConnected) {
                isConnected = false;
                stats.totalDisconnects++;
                Serial.println("NT: Disconnected from robot");
                if (connectionCallback) {
                    connectionCallback(false);
                }
            }
            break;
            
        case WStype_CONNECTED:
            isConnected = true;
            stats.connectTime = millis();
            Serial.printf("NT: Connected to robot at %s\n", (char*)payload);
            
            // Re-send all subscriptions
            for (auto& pair : subscriptions) {
                sendSubscription(pair.first, pair.second.dataType);
            }
            
            if (connectionCallback) {
                connectionCallback(true);
            }
            break;
            
        case WStype_TEXT:
            stats.messagesReceived++;
            processNTMessage(String((char*)payload));
            break;
            
        case WStype_ERROR:
            Serial.printf("NT: WebSocket error: %s\n", (char*)payload);
            break;
            
        default:
            break;
    }
}

void NetworkTablesSubscriber::processNTMessage(const String& message) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.printf("NT: JSON parse error: %s\n", error.c_str());
        return;
    }
    
    String method = doc["method"];
    
    if (method == "update") {
        JsonObject params = doc["params"];
        for (JsonPair kv : params) {
            String key = kv.key().c_str();
            auto it = subscriptions.find(key);
            
            if (it != subscriptions.end()) {
                String valueStr = kv.value().as<String>();
                it->second.callback(valueStr);
                it->second.lastUpdate = millis();
            }
        }
    }
    else if (method == "announce") {
        // Handle announcements if needed
    }
}

void NetworkTablesSubscriber::sendSubscription(const String& key, const String& dataType) {
    DynamicJsonDocument doc(256);
    doc["method"] = "subscribe";
    doc["params"]["topics"][0] = key;
    doc["params"]["subuid"] = nextSubscriptionId++;
    doc["params"]["options"]["periodic"] = 0.02; // 50Hz updates
    doc["params"]["options"]["all"] = false;
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    stats.messagesSent++;
}

void NetworkTablesSubscriber::sendUnsubscription(const String& key) {
    DynamicJsonDocument doc(256);
    doc["method"] = "unsubscribe";
    doc["params"]["subuid"] = nextSubscriptionId; // Use stored subscription ID in real implementation
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(message);
    stats.messagesSent++;
}

void NetworkTablesSubscriber::sendHeartbeat() {
    if (!isConnected) return;
    
    // Simple ping to keep connection alive
    webSocket.sendPing();
}

IPAddress NetworkTablesSubscriber::calculateRobotIP(int teamNumber) {
    if (teamNumber <= 0) {
        return IPAddress(10, 0, 0, 2); // Default
    }
    
    int firstOctet = teamNumber / 100;
    int secondOctet = teamNumber % 100;
    return IPAddress(10, firstOctet, secondOctet, 2);
}

unsigned long NetworkTablesSubscriber::getUptime() const {
    if (!isConnected || stats.connectTime == 0) return 0;
    return millis() - stats.connectTime;
}

void NetworkTablesSubscriber::getStatistics(unsigned long& uptime, unsigned long& disconnects, 
                                          unsigned long& messagesRx, unsigned long& messagesTx) {
    uptime = getUptime();
    disconnects = stats.totalDisconnects;
    messagesRx = stats.messagesReceived;
    messagesTx = stats.messagesSent;
}

void NetworkTablesSubscriber::printStatus() {
    Serial.println("=== NetworkTables Status ===");
    Serial.printf("Connected: %s\n", isConnected ? "YES" : "NO");
    Serial.printf("Robot IP: %s:%d\n", ipToString(robotIP).c_str(), ntPort);
    Serial.printf("Team Number: %d\n", teamNumber);
    Serial.printf("Uptime: %lu ms\n", getUptime());
    Serial.printf("Disconnects: %lu\n", stats.totalDisconnects);
    Serial.printf("Messages RX/TX: %lu/%lu\n", stats.messagesReceived, stats.messagesSent);
    Serial.printf("Subscriptions: %d\n", subscriptions.size());
    Serial.println("===========================");
}

void NetworkTablesSubscriber::printSubscriptions() {
    Serial.println("=== Active Subscriptions ===");
    for (auto& pair : subscriptions) {
        Serial.printf("Key: %s, Type: %s, Last Update: %lu ms ago\n", 
                     pair.first.c_str(), 
                     pair.second.dataType.c_str(),
                     millis() - pair.second.lastUpdate);
    }
    Serial.println("============================");
}

String NetworkTablesSubscriber::ipToString(IPAddress ip) {
    char ipStr[16];
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return String(ipStr);
}

#endif // NETWORKTABLES_SUBSCRIBER_H