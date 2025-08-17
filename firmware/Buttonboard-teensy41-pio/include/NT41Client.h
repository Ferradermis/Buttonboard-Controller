/*
 * NT4.1 Client for Teensy 4.1
 * 
 * OutlineViewer confirmed it's using protocol version 4.1, so let's implement
 * the correct NT4.1 protocol instead of generic NT4.
 */

#ifndef NT41_CLIENT_H
#define NT41_CLIENT_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>
#include <functional>

typedef std::function<void(bool connected)> ConnectionCallback;

class NT41Client {
private:
    IPAddress robotIP;
    uint16_t ntPort;
    EthernetClient client;
    bool isConnected;
    unsigned long connectionStartTime;
    unsigned long lastHeartbeat;
    int nextSubId;
    int nextPubId;
    bool handshakeComplete;
    
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
        Serial.println("NT4.1: " + message);
    }
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    bool performNT41WebSocketHandshake() {
        debugPrint("🤝 Starting NT4.1 WebSocket handshake...");
        
        // NT4.1 uses specific WebSocket subprotocol
        String request = "GET /nt/ws HTTP/1.1\r\n";
        request += "Host: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                   String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort) + "\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: dGVlbnN5LW50LWNsaWVudA==\r\n";
        request += "Sec-WebSocket-Version: 13\r\n";
        request += "Sec-WebSocket-Protocol: networktables.first.wpi.edu\r\n";
        request += "User-Agent: NT4.1-Teensy-Client\r\n";
        request += "\r\n";
        
        debugPrint("📤 Sending NT4.1 handshake request");
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
        
        debugPrint("📥 Handshake response (" + String(response.length()) + " bytes)");
        
        if (response.indexOf("101 Switching Protocols") >= 0) {
            debugPrint("✅ NT4.1 WebSocket handshake successful");
            handshakeComplete = true;
            return true;
        } else {
            debugPrint("❌ NT4.1 handshake failed");
            debugPrint("Response: " + response.substring(0, 200));
            return false;
        }
    }
    
    void sendNT41Message(const String& message) {
        if (!isConnected || !client.connected() || !handshakeComplete) {
            debugPrint("❌ Cannot send NT4.1 message - not ready");
            return;
        }
        
        size_t msgLen = message.length();
        
        // NT4.1 WebSocket frame with proper masking
        client.write(0x81); // Text frame, final fragment
        client.write((uint8_t)(msgLen | 0x80)); // Length with mask bit
        
        // Generate proper masking key for NT4.1
        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) {
            maskKey[i] = random(256);
        }
        client.write(maskKey, 4);
        
        // Send masked payload
        for (size_t i = 0; i < msgLen; i++) {
            client.write((uint8_t)(message[i] ^ maskKey[i % 4]));
        }
        
        client.flush();
        stats.messagesSent++;
        
        debugPrint("📤 NT4.1: " + message.substring(0, 100) + (message.length() > 100 ? "..." : ""));
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
                    debugPrint("📥 NT4.1: " + payload.substring(0, 150) + (payload.length() > 150 ? "..." : ""));
                    
                    parseNT41Message(payload);
                } else {
                    pos += payloadLen;
                }
            }
            
            bufferPos = 0;
        }
    }
    
    void parseNT41Message(const String& message) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            debugPrint("❌ NT4.1 JSON parse error: " + String(error.c_str()));
            return;
        }
        
        if (doc.is<JsonArray>()) {
            JsonArray messageArray = doc.as<JsonArray>();
            
            for (JsonObject messageObj : messageArray) {
                String method = messageObj["method"];
                
                if (method == "announce") {
                    handleNT41Announce(messageObj);
                } else if (method == "unannounce") {
                    String name = messageObj["params"]["name"];
                    debugPrint("📢 NT4.1 Topic removed: " + name);
                } else if (method == "setValues") {
                    handleNT41SetValues(messageObj);
                } else if (method == "update") {
                    handleNT41Update(messageObj);
                } else {
                    debugPrint("❓ NT4.1 Unknown method: " + method);
                }
            }
        }
    }
    
    void handleNT41Announce(JsonObject messageObj) {
        JsonObject params = messageObj["params"];
        String name = params["name"];
        String type = params["type"];
        int id = params["id"] | -1;
        
        debugPrint("📢 NT4.1 Announced: " + name + " (" + type + ") ID=" + String(id));
        
        // For any topic we're interested in, request its current value using NT4.1 format
        if (name.startsWith("/ControlBoard/") || name.startsWith("/SmartDashboard/")) {
            debugPrint("🔍 NT4.1 Requesting current value for: " + name);
            String valueRequest = "[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"" + name + "\"],\"subuid\":" + 
                                 String(nextSubId++) + ",\"options\":{\"immediate\":true,\"all\":false}}}]";
            delay(200); // Small delay for NT4.1
            sendNT41Message(valueRequest);
        }
    }
    
    void handleNT41SetValues(JsonObject messageObj) {
        JsonObject params = messageObj["params"];
        
        for (JsonPair kv : params) {
            String topic = kv.key().c_str();
            JsonVariant value = kv.value();
            
            debugPrint("📊 NT4.1 " + topic + " = " + value.as<String>());
            updateRobotData(topic, value);
        }
    }
    
    void handleNT41Update(JsonObject messageObj) {
        JsonObject params = messageObj["params"];
        
        for (JsonPair kv : params) {
            String topic = kv.key().c_str();
            JsonVariant value = kv.value();
            
            debugPrint("📈 NT4.1 " + topic + " = " + value.as<String>());
            updateRobotData(topic, value);
        }
    }
    
    void updateRobotData(const String& topic, JsonVariant value) {
        robotData.lastUpdate = millis();
        
        // ControlBoard data
        if (topic == "/ControlBoard/battery/voltage") {
            robotData.batteryVoltage = value.as<double>();
            debugPrint("🔋 NT4.1 Battery: " + String(robotData.batteryVoltage) + "V");
        } else if (topic == "/ControlBoard/robot/enabled") {
            robotData.robotEnabled = value.as<bool>();
            debugPrint("🤖 NT4.1 Robot " + String(robotData.robotEnabled ? "ENABLED" : "DISABLED"));
        } else if (topic == "/ControlBoard/robot/mode") {
            robotData.robotMode = value.as<String>();
            debugPrint("🎮 NT4.1 Mode: " + robotData.robotMode);
        } else if (topic == "/ControlBoard/shooter/ready") {
            robotData.shooterReady = value.as<bool>();
            if (robotData.shooterReady) debugPrint("🎯 NT4.1 Shooter READY");
        } else if (topic == "/ControlBoard/intake/deployed") {
            robotData.intakeDeployed = value.as<bool>();
        } else if (topic == "/ControlBoard/alliance/color") {
            robotData.allianceColor = value.as<String>();
            debugPrint("🏁 NT4.1 Alliance: " + robotData.allianceColor);
        } else if (topic == "/ControlBoard/auto/selectedMode") {
            robotData.autoMode = value.as<String>();
            debugPrint("🚗 NT4.1 Auto: " + robotData.autoMode);
        }
        
        // SmartDashboard data (as backup)
        else if (topic == "/SmartDashboard/BatteryVoltage") {
            robotData.batteryVoltage = value.as<double>();
            debugPrint("🔋 NT4.1 Battery (SD): " + String(robotData.batteryVoltage) + "V");
        } else if (topic == "/SmartDashboard/RobotEnabled") {
            robotData.robotEnabled = value.as<bool>();
        } else if (topic == "/SmartDashboard/ShooterReady") {
            robotData.shooterReady = value.as<bool>();
        }
    }
    
    void establishNT41Connection() {
        debugPrint("🔄 Establishing NT4.1 bidirectional connection...");
        
        // Step 1: Announce ourselves as a publisher (NT4.1 style)
        sendNT41Message("[{\"method\":\"announce\",\"params\":{\"name\":\"/SmartDashboard/TeensyNT41\",\"type\":\"boolean\",\"pubuid\":" + String(nextPubId++) + ",\"properties\":{}}}]");
        delay(300);
        
        // Step 2: Publish a value
        sendNT41Message("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyNT41\":true}}]");
        delay(300);
        
        // Step 3: Subscribe to ControlBoard data (NT4.1 format)
        sendNT41Message("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/ControlBoard/\"],\"subuid\":" + String(nextSubId++) + ",\"options\":{\"all\":false,\"topicsOnly\":false,\"prefix\":true}}}]");
        delay(300);
        
        // Step 4: Subscribe to SmartDashboard data
        sendNT41Message("[{\"method\":\"subscribe\",\"params\":{\"topics\":[\"/SmartDashboard/\"],\"subuid\":" + String(nextSubId++) + ",\"options\":{\"all\":false,\"topicsOnly\":false,\"prefix\":true}}}]");
        
        debugPrint("✅ NT4.1 bidirectional connection established");
    }
    
public:
    NT41Client(int teamNumber = 6574) :
        robotIP(calculateRobotIP(teamNumber)),
        ntPort(5810), // NT4.1 uses same port as NT4
        isConnected(false),
        connectionStartTime(0),
        lastHeartbeat(0),
        nextSubId(1),
        nextPubId(1),
        handshakeComplete(false),
        connectionCallback(nullptr)
    {
        memset(&stats, 0, sizeof(stats));
    }
    
    void setConnectionCallback(ConnectionCallback callback) {
        connectionCallback = callback;
    }
    
    bool begin() {
        debugPrint("=== Starting NT4.1 Connection ===");
        debugPrint("Protocol version 4.1 (same as OutlineViewer)");
        
        if (client.connect(robotIP, ntPort)) {
            debugPrint("✅ TCP connected to " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                      String(robotIP[2]) + "." + String(robotIP[3]) + ":" + String(ntPort));
            
            if (performNT41WebSocketHandshake()) {
                debugPrint("✅ NT4.1 WebSocket ready");
                
                isConnected = true;
                connectionStartTime = millis();
                stats.connectTime = millis();
                stats.totalConnections++;
                
                if (connectionCallback) {
                    connectionCallback(true);
                }
                
                // Wait a moment then establish NT4.1 connection
                delay(1000);
                establishNT41Connection();
                
                lastHeartbeat = millis();
                
                debugPrint("🎉 NT4.1 connection fully established!");
                return true;
            } else {
                client.stop();
            }
        }
        
        debugPrint("❌ NT4.1 connection failed");
        return false;
    }
    
    void update() {
        if (!isConnected) return;
        
        if (!client.connected()) {
            debugPrint("💔 NT4.1 connection lost");
            isConnected = false;
            handshakeComplete = false;
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
            sendNT41Message("[{\"method\":\"setValues\",\"params\":{\"/SmartDashboard/TeensyNT41Heartbeat\":" + String(now) + "}}]");
            lastHeartbeat = now;
        }
    }
    
    // Data accessors (same API)
    bool connected() const { return isConnected && handshakeComplete; }
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
        debugPrint("=== NT4.1 Status ===");
        debugPrint("Connected: " + String(isConnected ? "YES" : "NO"));
        debugPrint("Handshake: " + String(handshakeComplete ? "COMPLETE" : "PENDING"));
        if (isConnected) {
            debugPrint("Uptime: " + String(millis() - connectionStartTime) + "ms");
            debugPrint("Messages RX/TX: " + String(stats.messagesReceived) + "/" + String(stats.messagesSent));
            if (robotData.lastUpdate > 0) {
                debugPrint("Battery: " + String(robotData.batteryVoltage) + "V");
                debugPrint("Data age: " + String(millis() - robotData.lastUpdate) + "ms");
            }
        }
        debugPrint("====================");
    }
};

#endif // NT41_CLIENT_H