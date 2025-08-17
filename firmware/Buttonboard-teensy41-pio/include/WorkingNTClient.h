/*
 * Working NT4 Client - Final Implementation
 * 
 * Based on successful bidirectional test, this implements:
 * 1. Publish first to establish client as active
 * 2. Subscribe with leading slash format
 * 3. Handle both announce and value messages
 * 4. Request current values after announcements
 */

#ifndef WORKING_NT_CLIENT_H
#define WORKING_NT_CLIENT_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>
#include <functional>

typedef std::function<void(bool connected)> ConnectionCallback;

class WorkingNTClient {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    unsigned long lastHeartbeat;
    int nextSubId;
    int nextPubId;
    
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
        Serial.println("NT: " + message);
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
        
        size_t msgLen = message.length();
        
        // WebSocket frame with masking
        client.write(0x81); // Text frame
        client.write((uint8_t)(msgLen | 0x80)); // Length with mask bit
        
        // Generate masking key
        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) {
            maskKey[i] = random(256);
        }
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
        
        // Read available data
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
                    debugPrint("📥 Received: " + payload.substring(0, 100) + (payload.length() > 100 ? "..." : ""));
                    
                    parseNTMessage(payload);
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
    void parseNTMessage(const String& message) {
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
        
        // For any topic we're interested in, request its current value
        if (name.startsWith("/ControlBoard/") || name.startsWith("/SmartDashboard/")) {
            debugPrint("🔍 Requesting current value for: " + name);
            // Request current value by subscribing with immediate=true
            String valueRequest = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"" + name + "\"],\"subuid\":" + 
                                 String(nextSubId++) + ",\"options\":{\"immediate\":true}}}]";
            delay(100); // Small delay to avoid overwhelming
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
        // Similar to setValues but different format
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
            debugPrint("🔋 Battery: " + String(robotData.batteryVoltage) + "V");
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
    
    void establishBidirectionalConnection() {
        debugPrint("🔄 Establishing bidirectional connection...");
        
        // Step 1: Announce ourselves as a publisher
        sendMaskedMessage("[{\"method\":\"announce\",\"params\":{\"name\":\"/SmartDashboard/TeensyAlive\",\"type\":\"boolean\",\"pubuid\":" + String(nextPubId++) + "}}]");
        delay(200);
        
        // Step 2: Publish a value
        sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyAlive\":true}}]");
        delay(200);
        
        // Step 3: Subscribe to ControlBoard data
        sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
        delay(200);
        
        // Step 4: Subscribe to SmartDashboard data
        sendMaskedMessage("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/\"],\"subuid\":" + String(nextSubId++) + "}}]");
        

        delay(200);

        debugPrint("✅ Bidirectional connection established");
    }
    
public:
    WorkingNTClient(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810),
        isConnected(false),
        connectionStartTime(0),
        lastHeartbeat(0),
        nextSubId(1),
        nextPubId(1),
        connectionCallback(nullptr)
    {
        memset(&stats, 0, sizeof(stats));
    }
    
    void setConnectionCallback(ConnectionCallback callback) {
        connectionCallback = callback;
    }
    
    bool begin() {
        debugPrint("=== Starting NT4 Connection ===");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected");
            
            if (performWebSocketHandshake()) {
                debugPrint("✅ WebSocket ready");
                
                isConnected = true;
                connectionStartTime = millis();
                stats.connectTime = millis();
                stats.totalConnections++;
                
                if (connectionCallback) {
                    connectionCallback(true);
                }
                
                // Wait a moment then establish bidirectional connection
                delay(1000);
                establishBidirectionalConnection();
                
                lastHeartbeat = millis();
                
                debugPrint("🎉 NT4 connection fully established!");
                return true;
            } else {
                client.stop();
            }
        }
        
        debugPrint("❌ Connection failed");
        return false;
    }
    
    void update() {
        if (!isConnected) return;
        
        if (!client.connected()) {
            debugPrint("💔 Connection lost");
            isConnected = false;
            stats.totalDisconnects++;
            
            if (connectionCallback) {
                connectionCallback(false);
            }
            return;
        }
        
        // Process incoming data
        processIncomingData();
        
        // Send periodic heartbeat
        unsigned long now = millis();
        if (now - lastHeartbeat >= 5000) { // Every 5 seconds
            sendMaskedMessage("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyHeartbeat\":" + String(now) + "}}]");
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
        debugPrint("=== NT4 Status ===");
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

#endif // WORKING_NT_CLIENT_H