/*
 * Simple NetworkTables Client for Teensy 4.1
 * Uses raw TCP connection instead of WebSockets to avoid library conflicts
 * Compatible with NetworkTables 4.0+ protocol
 */

#ifndef SIMPLE_NT_CLIENT_H
#define SIMPLE_NT_CLIENT_H

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

// Subscription structure
struct NTSubscription {
    String key;
    String dataType;
    std::function<void(const String&)> callback;
    uint32_t lastUpdate;
    bool active;
};

class SimpleNTClient {
private:
    // Network configuration
    int teamNumber;
    IPAddress robotIP;
    uint16_t ntPort;
    
    // TCP client
    EthernetClient client;
    bool isConnected;
    bool autoReconnect;
    
    // Connection management
    unsigned long lastConnectionAttempt;
    unsigned long lastHeartbeat;
    unsigned long heartbeatInterval;
    
    // Subscriptions
    std::map<String, NTSubscription> subscriptions;
    uint32_t nextSubId;
    
    // Callbacks
    ConnectionCallback connectionCallback;
    
    // Statistics
    struct {
        unsigned long connectTime;
        unsigned long totalDisconnects;
        unsigned long messagesReceived;
        unsigned long messagesSent;
    } stats;
    
    // Message handling
    void processIncomingData();
    void sendMessage(const String& message);
    void sendHeartbeat();
    IPAddress calculateRobotIP(int teamNumber);
    
    // Data conversion utilities
    double stringToDouble(const String& str) { return atof(str.c_str()); }
    bool stringToBool(const String& str) {
        return (str.equalsIgnoreCase("true") || str == "1" || atoi(str.c_str()) != 0);
    }

public:
    SimpleNTClient(int teamNumber = 0);
    ~SimpleNTClient();
    
    // Connection management
    bool begin();
    bool begin(IPAddress customIP, uint16_t port = 5810);
    void setTeamNumber(int team);
    void setAutoReconnect(bool enable);
    void setConnectionCallback(ConnectionCallback callback);
    
    // Main update loop
    void update();
    
    // Connection status
    bool connected() const { return isConnected; }
    unsigned long getUptime() const;
    
    // Subscription management
    bool subscribeNumber(const String& key, NumberCallback callback);
    bool subscribeBoolean(const String& key, BooleanCallback callback);
    bool subscribeString(const String& key, StringCallback callback);
    bool unsubscribe(const String& key);
    void unsubscribeAll();
    
    // Publishing
    bool publishNumber(const String& key, double value);
    bool publishBoolean(const String& key, bool value);
    bool publishString(const String& key, const String& value);
    
    // Utility
    void printStatus();
    void printSubscriptions();
};

// Implementation

SimpleNTClient::SimpleNTClient(int teamNumber) :
    teamNumber(teamNumber),
    ntPort(5810),
    isConnected(false),
    autoReconnect(true),
    lastConnectionAttempt(0),
    lastHeartbeat(0),
    heartbeatInterval(5000),
    nextSubId(1),
    connectionCallback(nullptr)
{
    robotIP = calculateRobotIP(teamNumber);
    memset(&stats, 0, sizeof(stats));
}

SimpleNTClient::~SimpleNTClient() {
    if (client.connected()) {
        client.stop();
    }
}

bool SimpleNTClient::begin() {
    if (teamNumber == 0) {
        Serial.println("NT: Error - Team number not set");
        return false;
    }
    return begin(robotIP, ntPort);
}

bool SimpleNTClient::begin(IPAddress customIP, uint16_t port) {
    robotIP = customIP;
    ntPort = port;
    
    Serial.printf("NT: Attempting connection to %d.%d.%d.%d:%d\n", 
                 robotIP[0], robotIP[1], robotIP[2], robotIP[3], ntPort);
    
    if (client.connect(robotIP, ntPort)) {
        isConnected = true;
        stats.connectTime = millis();
        Serial.println("NT: Connected to robot");
        
        // Send initial handshake
        sendMessage("{\"method\":\"subscribe\",\"params\":{\"topics\":[],\"options\":{\"all\":true}}}");
        
        // Re-send all subscriptions
        for (auto& pair : subscriptions) {
            String subMsg = "{\"method\":\"subscribe\",\"params\":{\"topics\":[\"" + 
                           pair.first + "\"],\"subuid\":" + String(nextSubId++) + "}}";
            sendMessage(subMsg);
        }
        
        if (connectionCallback) {
            connectionCallback(true);
        }
        
        lastConnectionAttempt = millis();
        return true;
    } else {
        Serial.println("NT: Failed to connect to robot");
        isConnected = false;
        lastConnectionAttempt = millis();
        return false;
    }
}

void SimpleNTClient::setTeamNumber(int team) {
    teamNumber = team;
    robotIP = calculateRobotIP(team);
}

void SimpleNTClient::setAutoReconnect(bool enable) {
    autoReconnect = enable;
}

void SimpleNTClient::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback = callback;
}

void SimpleNTClient::update() {
    unsigned long currentTime = millis();
    
    // Check connection status
    if (isConnected && !client.connected()) {
        // Connection lost
        isConnected = false;
        stats.totalDisconnects++;
        Serial.println("NT: Connection lost");
        
        if (connectionCallback) {
            connectionCallback(false);
        }
    }
    
    // Process incoming data if connected
    if (isConnected && client.connected()) {
        processIncomingData();
        
        // Send periodic heartbeat
        if (currentTime - lastHeartbeat >= heartbeatInterval) {
            sendHeartbeat();
            lastHeartbeat = currentTime;
        }
    }
    
    // Auto-reconnect if enabled and disconnected
    if (!isConnected && autoReconnect && 
        (currentTime - lastConnectionAttempt >= 5000)) {
        Serial.println("NT: Attempting reconnection...");
        begin(robotIP, ntPort);
    }
}

bool SimpleNTClient::subscribeNumber(const String& key, NumberCallback callback) {
    NTSubscription sub;
    sub.key = key;
    sub.dataType = "double";
    sub.callback = [key, callback, this](const String& valueStr) {
        double value = this->stringToDouble(valueStr);
        callback(key, value);
    };
    sub.lastUpdate = millis();
    sub.active = true;
    
    subscriptions[key] = sub;
    
    if (isConnected) {
        String subMsg = "{\"method\":\"subscribe\",\"params\":{\"topics\":[\"" + 
                       key + "\"],\"subuid\":" + String(nextSubId++) + "}}";
        sendMessage(subMsg);
    }
    
    Serial.printf("NT: Subscribed to number: %s\n", key.c_str());
    return true;
}

bool SimpleNTClient::subscribeBoolean(const String& key, BooleanCallback callback) {
    NTSubscription sub;
    sub.key = key;
    sub.dataType = "boolean";
    sub.callback = [key, callback, this](const String& valueStr) {
        bool value = this->stringToBool(valueStr);
        callback(key, value);
    };
    sub.lastUpdate = millis();
    sub.active = true;
    
    subscriptions[key] = sub;
    
    if (isConnected) {
        String subMsg = "{\"method\":\"subscribe\",\"params\":{\"topics\":[\"" + 
                       key + "\"],\"subuid\":" + String(nextSubId++) + "}}";
        sendMessage(subMsg);
    }
    
    Serial.printf("NT: Subscribed to boolean: %s\n", key.c_str());
    return true;
}

bool SimpleNTClient::subscribeString(const String& key, StringCallback callback) {
    NTSubscription sub;
    sub.key = key;
    sub.dataType = "string";
    sub.callback = [key, callback](const String& valueStr) {
        callback(key, valueStr);
    };
    sub.lastUpdate = millis();
    sub.active = true;
    
    subscriptions[key] = sub;
    
    if (isConnected) {
        String subMsg = "{\"method\":\"subscribe\",\"params\":{\"topics\":[\"" + 
                       key + "\"],\"subuid\":" + String(nextSubId++) + "}}";
        sendMessage(subMsg);
    }
    
    Serial.printf("NT: Subscribed to string: %s\n", key.c_str());
    return true;
}

bool SimpleNTClient::unsubscribe(const String& key) {
    auto it = subscriptions.find(key);
    if (it != subscriptions.end()) {
        subscriptions.erase(it);
        Serial.printf("NT: Unsubscribed from: %s\n", key.c_str());
        return true;
    }
    return false;
}

void SimpleNTClient::unsubscribeAll() {
    subscriptions.clear();
    Serial.println("NT: Unsubscribed from all keys");
}

bool SimpleNTClient::publishNumber(const String& key, double value) {
    if (!isConnected) return false;
    
    String msg = "{\"method\":\"update\",\"params\":{\"" + key + "\":" + String(value, 6) + "}}";
    sendMessage(msg);
    return true;
}

bool SimpleNTClient::publishBoolean(const String& key, bool value) {
    if (!isConnected) return false;
    
    String msg = "{\"method\":\"update\",\"params\":{\"" + key + "\":" + (value ? "true" : "false") + "}}";
    sendMessage(msg);
    return true;
}

bool SimpleNTClient::publishString(const String& key, const String& value) {
    if (!isConnected) return false;
    
    String msg = "{\"method\":\"update\",\"params\":{\"" + key + "\":\"" + value + "\"}}";
    sendMessage(msg);
    return true;
}

void SimpleNTClient::processIncomingData() {
    String buffer = "";
    
    while (client.available()) {
        char c = client.read();
        buffer += c;
        
        // Look for complete JSON messages (simple newline-delimited approach)
        if (c == '\n' || c == '\r') {
            buffer.trim();
            if (buffer.length() > 0) {
                // Parse JSON message
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, buffer);
                
                if (!error) {
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
                                stats.messagesReceived++;
                            }
                        }
                    }
                }
                buffer = "";
            }
        }
    }
}

void SimpleNTClient::sendMessage(const String& message) {
    if (isConnected && client.connected()) {
        client.print(message);
        client.print("\n");
        stats.messagesSent++;
    }
}

void SimpleNTClient::sendHeartbeat() {
    if (isConnected) {
        // Simple ping message
        sendMessage("{\"method\":\"ping\"}");
    }
}

IPAddress SimpleNTClient::calculateRobotIP(int teamNumber) {
    if (teamNumber <= 0) {
        return IPAddress(10, 0, 0, 2);
    }
    
    int firstOctet = teamNumber / 100;
    int secondOctet = teamNumber % 100;
    return IPAddress(10, firstOctet, secondOctet, 2);
}

unsigned long SimpleNTClient::getUptime() const {
    if (!isConnected || stats.connectTime == 0) return 0;
    return millis() - stats.connectTime;
}

void SimpleNTClient::printStatus() {
    Serial.println("=== NetworkTables Status ===");
    Serial.printf("Connected: %s\n", isConnected ? "YES" : "NO");
    Serial.printf("Robot IP: %d.%d.%d.%d:%d\n", robotIP[0], robotIP[1], robotIP[2], robotIP[3], ntPort);
    Serial.printf("Team Number: %d\n", teamNumber);
    Serial.printf("Uptime: %lu ms\n", getUptime());
    Serial.printf("Disconnects: %lu\n", stats.totalDisconnects);
    Serial.printf("Messages RX/TX: %lu/%lu\n", stats.messagesReceived, stats.messagesSent);
    Serial.printf("Subscriptions: %d\n", subscriptions.size());
    Serial.println("===========================");
}

void SimpleNTClient::printSubscriptions() {
    Serial.println("=== Active Subscriptions ===");
    for (auto& pair : subscriptions) {
        Serial.printf("Key: %s, Type: %s, Last Update: %lu ms ago\n", 
                     pair.first.c_str(), 
                     pair.second.dataType.c_str(),
                     millis() - pair.second.lastUpdate);
    }
    Serial.println("============================");
}

#endif // SIMPLE_NT_CLIENT_H