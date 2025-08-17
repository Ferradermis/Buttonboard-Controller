/*
 * Final Working NT4 Client for Teensy 4.1
 * 
 * Based on successful test results - uses the exact subscription pattern
 * that worked: specific topic subscriptions with individual subuid values.
 */

#ifndef FINAL_WORKING_NT4_CLIENT_H
#define FINAL_WORKING_NT4_CLIENT_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>
#include <functional>

typedef std::function<void(bool connected)> ConnectionCallback;

class FinalWorkingNT4Client {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    unsigned long lastHeartbeat;
    int nextSubId;
    bool subscriptionsActive;
    
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
        unsigned long lastDataTime;
    } stats;
    
    void debugPrint(const String& message) {
        Serial.println("NT4: " + message);
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
            return true;
        } else {
            debugPrint("❌ WebSocket handshake failed");
            return false;
        }
    }
    
    void sendMaskedMessage(const String& message) {
        if (!isConnected || !client.connected()) {
            debugPrint("❌ Cannot send - not connected");
            return;
        }
        
        size_t msgLen = message.length();
        
        // WebSocket frame with masking (proven format)
        client.write(0x81); // Text frame
        client.write((uint8_t)(msgLen | 0x80)); // Length with mask bit
        
        // Fixed masking key for consistency
        uint8_t maskKey[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        client.write(maskKey, 4);
        
        // Masked payload
        for (size_t i = 0; i < msgLen; i++) {
            client.write((uint8_t)(message[i] ^ maskKey[i % 4]));
        }
        
        client.flush();
        stats.messagesSent++;
        
        debugPrint("📤 Sent: " + message.substring(0, 80) + (message.length() > 80 ? "..." : ""));
    }
    
    void processIncomingData() {
        static uint8_t buffer[2048];
        static size_t bufferPos = 0;
        
        while (client.available() && bufferPos < sizeof(buffer) - 1) {
            buffer[bufferPos++] = client.read();
        }
        
        if (bufferPos > 0) {
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
                    
                    stats.messagesReceived++;
                    stats.lastDataTime = millis();
                    
                    debugPrint("📥 Received: " + payload.substring(0, 100) + (payload.length() > 100 ? "..." : ""));
                    
                    parseNT4Message(payload);
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
    void parseNT4Message(const String& message) {
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
                    handleAnnounce(messageObj);
                } else if (method == "unannounce") {
                    String name = messageObj["params"]["name"];
                    debugPrint("📢 Topic removed: " + name);
                } else if (method == "setValues") {
                    handleSetValues(messageObj);
                } else if (method == "update") {
                    handleUpdate(messageObj);
                } else {
                    debugPrint("❓ Unknown method: " + method);
                }
            }
        }
    }
    
    void handleAnnounce(JsonObject messageObj) {
        JsonObject params = messageObj["params"];
        String name = params["name"];
        String type = params["type"];
        int id = params["id"] | -1;
        
        debugPrint("📢 Announced: " + name + " (" + type + ") ID=" + String(id));
        
        // For announced topics, immediately request current value using working pattern
        if (name.indexOf("ControlBoard") >= 0 || name.indexOf("SmartDashboard") >= 0) {
            debugPrint("🔍 Requesting current value for: " + name);
            // Use the exact successful subscription pattern with immediate option
            String valueRequest = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"" + name + "\"],\"subuid\":" + 
                                 String(nextSubId++) + ",\"options\":{\"immediate\":true}}}]";
            delay(500); // Proven delay timing
            sendMaskedMessage(valueRequest);
        }
    }
    
    void handleSetValues(JsonObject messageObj) {
        JsonObject params = messageObj["params"];
        
        for (JsonPair kv : params) {
            String topic = kv.key().c_str();
            JsonVariant value = kv.value();
            
            debugPrint("📊 " + topic + " = " + value.as<String>());
            updateRobotData(topic, value);
        }
    }
    
    void handleUpdate(JsonObject messageObj) {
        JsonObject params = messageObj["params"];
        
        for (JsonPair kv : params) {
            String topic = kv.key().c_str();
            JsonVariant value = kv.value();
            
            debugPrint("📈 " + topic + " = " + value.as<String>());
            updateRobotData(topic, value);
        }
    }
    
    void updateRobotData(const String& topic, JsonVariant value) {
        robotData.lastUpdate = millis();
        
        // ControlBoard data
        if (topic == "/ControlBoard/battery/voltage") {
            robotData.batteryVoltage = value.as<double>();
            debugPrint("🔋 *** BATTERY VOLTAGE: " + String(robotData.batteryVoltage) + "V ***");
        } else if (topic == "/ControlBoard/robot/enabled") {
            robotData.robotEnabled = value.as<bool>();
            debugPrint("🤖 Robot " + String(robotData.robotEnabled ? "ENABLED" : "DISABLED"));
        } else if (topic == "/ControlBoard/robot/mode") {
            robotData.robotMode = value.as<String>();
            debugPrint("🎮 Mode: " + robotData.robotMode);
        } else if (topic == "/ControlBoard/shooter/ready") {
            robotData.shooterReady = value.as<bool>();
            if (robotData.shooterReady) debugPrint("🎯 Shooter READY");
        } else if (topic == "/ControlBoard/intake/deployed") {
            robotData.intakeDeployed = value.as<bool>();
        } else if (topic == "/ControlBoard/alliance/color") {
            robotData.allianceColor = value.as<String>();
            debugPrint("🏁 Alliance: " + robotData.allianceColor);
        } else if (topic == "/ControlBoard/auto/selectedMode") {
            robotData.autoMode = value.as<String>();
            debugPrint("🚗 Auto: " + robotData.autoMode);
        } else if (topic == "/ControlBoard/drive/fieldOriented") {
            robotData.fieldOriented = value.as<bool>();
        } else if (topic == "/ControlBoard/match/timeRemaining") {
            robotData.matchTimeRemaining = value.as<double>();
        } else if (topic == "/ControlBoard/status/message") {
            robotData.statusMessage = value.as<String>();
        } else if (topic == "/ControlBoard/robot/heartbeat") {
            robotData.heartbeat = value.as<int>();
        }
        
        // SmartDashboard data (as backup)
        else if (topic == "/SmartDashboard/BatteryVoltage") {
            robotData.batteryVoltage = value.as<double>();
            debugPrint("🔋 Battery (SD): " + String(robotData.batteryVoltage) + "V");
        } else if (topic == "/SmartDashboard/RobotEnabled") {
            robotData.robotEnabled = value.as<bool>();
        } else if (topic == "/SmartDashboard/ShooterReady") {
            robotData.shooterReady = value.as<bool>();
        }
    }
    
    void setupSubscriptions() {
        debugPrint("📡 Setting up subscriptions using proven pattern...");
        
        // Use the exact successful pattern from the test
        // Subscribe to specific ControlBoard topics that we know exist
        
        delay(2000); // Wait for connection to stabilize
        
        debugPrint("📡 Subscribing to battery voltage...");
        sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/battery/voltage\"],\"subuid\":" + String(nextSubId++) + "}}]");
        
        delay(1000);
        
        debugPrint("📡 Subscribing to robot state...");
        sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/robot/enabled\"],\"subuid\":" + String(nextSubId++) + "}}]");
        
        delay(1000);
        
        debugPrint("📡 Subscribing to SmartDashboard backup...");
        sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/BatteryVoltage\"],\"subuid\":" + String(nextSubId++) + "}}]");
        
        subscriptionsActive = true;
        debugPrint("✅ Subscriptions setup complete");
    }
    
public:
    FinalWorkingNT4Client(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        lastHeartbeat(0),
        nextSubId(1),
        subscriptionsActive(false),
        connectionCallback(nullptr)
    {
        memset(&stats, 0, sizeof(stats));
    }
    
    void setConnectionCallback(ConnectionCallback callback) {
        connectionCallback = callback;
    }
    
    bool begin() {
        debugPrint("=== Starting Final Working NT4 Client ===");
        debugPrint("Using proven successful patterns from tests");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected to " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                      String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort));
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                
                isConnected = true;
                connectionStartTime = millis();
                stats.connectTime = millis();
                stats.totalConnections++;
                
                if (connectionCallback) {
                    connectionCallback(true);
                }
                
                // Setup subscriptions using proven pattern
                setupSubscriptions();
                
                lastHeartbeat = millis();
                
                debugPrint("🎉 Final NT4 client ready - should receive battery data!");
                return true;
            } else {
                client.stop();
            }
        }
        
        debugPrint("❌ NT4 connection failed");
        return false;
    }
    
    void update() {
        if (!isConnected) return;
        
        if (!client.connected()) {
            debugPrint("💔 Connection lost");
            isConnected = false;
            subscriptionsActive = false;
            stats.totalDisconnects++;
            
            if (connectionCallback) {
                connectionCallback(false);
            }
            return;
        }
        
        // Process incoming data
        processIncomingData();
        
        // Minimal heartbeat to keep connection alive
        unsigned long now = millis();
        if (now - lastHeartbeat >= 30000) { // Every 30 seconds
            debugPrint("💓 Connection alive for " + String((now - connectionStartTime) / 1000) + "s");
            lastHeartbeat = now;
        }
    }
    
    // Data accessors
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
        debugPrint("=== Final NT4 Status ===");
        debugPrint("Connected: " + String(isConnected ? "YES" : "NO"));
        debugPrint("Subscriptions: " + String(subscriptionsActive ? "ACTIVE" : "INACTIVE"));
        if (isConnected) {
            debugPrint("Uptime: " + String(millis() - connectionStartTime) + "ms");
            debugPrint("Messages RX/TX: " + String(stats.messagesReceived) + "/" + String(stats.messagesSent));
            debugPrint("*** BATTERY: " + String(robotData.batteryVoltage) + "V ***");
            if (robotData.lastUpdate > 0) {
                debugPrint("Data age: " + String(millis() - robotData.lastUpdate) + "ms");
            } else {
                debugPrint("⚠️  No robot data received yet");
            }
        }
        debugPrint("========================");
    }
};

#endif // FINAL_WORKING_NT4_CLIENT_H