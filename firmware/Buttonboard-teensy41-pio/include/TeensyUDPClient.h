/*
 * Teensy UDP Client - Simple Telemetry Receiver
 * 
 * Receives robot telemetry via UDP - MUCH simpler than NetworkTables!
 */

#ifndef TEENSY_UDP_CLIENT_H
#define TEENSY_UDP_CLIENT_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <EthernetUdp.h>
#include <ArduinoJson.h>

class TeensyUDPClient {
private:
    EthernetUDP udp;
    uint16_t localPort;
    IPAddress robotIP;
    int teamNumber;
    unsigned long lastPacketTime;
    int packetsReceived;
    
    // Discovery and heartbeat
    unsigned long lastDiscoveryTime;
    unsigned long discoveryInterval;
    bool robotKnowsAboutUs;
    
    // Robot telemetry data
    struct RobotData {
        double timestamp = 0.0;
        
        // Battery
        double batteryVoltage = 0.0;
        bool batteryIsLow = false;
        
        // Robot state
        bool robotEnabled = false;
        bool isAutonomous = false;
        bool isTeleop = false;
        String robotMode = "Unknown";
        
        // Alliance
        String allianceColor = "unknown";
        bool isRedAlliance = false;
        
        // Match
        double matchTimeRemaining = 0.0;
        
        // Subsystems
        bool shooterReady = false;
        int shooterSpeed = 0;
        bool intakeDeployed = false;
        double intakePosition = 0.0;
        
        // Auto
        String autoMode = "Unknown";
        int autoModeNumber = 0;
        
        // Status
        String statusMessage = "";
        int heartbeat = 0;
        
        unsigned long lastUpdate = 0;

        bool leftCamHasTag = false;
        float leftCamTagID=-1;
        bool rightCamHasTag = false;
        float rightCamTagID=-1;

        /**
         * Compare meaningful fields (ignore timestamp, heartbeat, lastUpdate)
         * Returns true if any significant data has changed
         */
        bool hasSignificantChanges(const RobotData& other) const {
            return (abs(batteryVoltage - other.batteryVoltage) > 0.01) ||
                   (batteryIsLow != other.batteryIsLow) ||
                   (robotEnabled != other.robotEnabled) ||
                   (isAutonomous != other.isAutonomous) ||
                   (isTeleop != other.isTeleop) ||
                   (robotMode != other.robotMode) ||
                   (allianceColor != other.allianceColor) ||
                   (isRedAlliance != other.isRedAlliance) ||
                   (abs(matchTimeRemaining - other.matchTimeRemaining) > 0.1) ||
                   (shooterReady != other.shooterReady) ||
                   (shooterSpeed != other.shooterSpeed) ||
                   (intakeDeployed != other.intakeDeployed) ||
                   (abs(intakePosition - other.intakePosition) > 0.01) ||
                   (autoMode != other.autoMode) ||
                   (autoModeNumber != other.autoModeNumber) ||
                   (statusMessage != other.statusMessage) ||
                   (leftCamHasTag != other.leftCamHasTag) ||
                   (leftCamTagID != other.leftCamTagID) ||
                   (rightCamHasTag != other.rightCamHasTag) ||
                   (rightCamTagID != other.rightCamTagID);
        }
        
        /**
         * Copy meaningful data from another RobotData (preserves timestamps)
         */
        void copySignificantData(const RobotData& source) {
            batteryVoltage = source.batteryVoltage;
            batteryIsLow = source.batteryIsLow;
            robotEnabled = source.robotEnabled;
            isAutonomous = source.isAutonomous;
            isTeleop = source.isTeleop;
            robotMode = source.robotMode;
            allianceColor = source.allianceColor;
            isRedAlliance = source.isRedAlliance;
            matchTimeRemaining = source.matchTimeRemaining;
            shooterReady = source.shooterReady;
            shooterSpeed = source.shooterSpeed;
            intakeDeployed = source.intakeDeployed;
            intakePosition = source.intakePosition;
            autoMode = source.autoMode;
            autoModeNumber = source.autoModeNumber;
            statusMessage = source.statusMessage;
            leftCamHasTag = source.leftCamHasTag;
            leftCamTagID = source.leftCamTagID;
            rightCamHasTag = source.rightCamHasTag;
            rightCamTagID = source.rightCamTagID;
        }
    } robotData;
    
    // Previous state for change detection
    RobotData previousRobotData;
    bool hasNewChangedData = false;
    
    IPAddress calculateRobotIP(int teamNumber) {
        if (teamNumber <= 0) return IPAddress(10, 0, 0, 2);
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return IPAddress(10, firstOctet, secondOctet, 2);
    }
    
    void debugPrint(const String& message) {
        Serial.println("UDP: " + message);
    }
    
    void sendDiscoveryMessage() {
        // Send "here I am" message to robot
        String discoveryMsg = "{\"type\":\"discovery\",\"device\":\"teensy_control_board\",\"ip\":\"";
        discoveryMsg += String(Ethernet.localIP()[0]) + "." + String(Ethernet.localIP()[1]) + "." + 
                       String(Ethernet.localIP()[2]) + "." + String(Ethernet.localIP()[3]);
        discoveryMsg += "\",\"port\":" + String(localPort);
        discoveryMsg += ",\"team\":" + String(teamNumber);
        discoveryMsg += ",\"timestamp\":" + String(millis());
        discoveryMsg += "}";
        
        // Send to robot's discovery port (robot should listen on multiple ports)
        udp.beginPacket(robotIP, 5575); // Discovery port
        udp.print(discoveryMsg);
        udp.endPacket();
        
        debugPrint("📡 Sent discovery: " + discoveryMsg);
    }
    
    void parseRobotData(const String& jsonData) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, jsonData);
        
        if (error) {
            debugPrint("❌ JSON parse error: " + String(error.c_str()));
            return;
        }
        

        debugPrint(jsonData);

        // Store previous data for change detection
        previousRobotData.copySignificantData(robotData);
        
        robotData.lastUpdate = millis();
        
        // Parse timestamp
        if (doc["timestamp"]) {
            robotData.timestamp = doc["timestamp"];
        }
        
        // Parse battery data
        if (doc["battery"]) {
            robotData.batteryVoltage = doc["battery"]["voltage"] | 0.0;
            robotData.batteryIsLow = doc["battery"]["isLow"] | false;
            
            debugPrint("🔋 Battery: " + String(robotData.batteryVoltage) + "V");
        }
        
        // Parse robot state
        if (doc["robot"]) {
            robotData.robotEnabled = doc["robot"]["enabled"] | false;
            robotData.isAutonomous = doc["robot"]["autonomous"] | false;
            robotData.isTeleop = doc["robot"]["teleop"] | false;
            robotData.robotMode = doc["robot"]["mode"] | "Unknown";
            
            debugPrint("🤖 Robot: " + String(robotData.robotEnabled ? "ENABLED" : "DISABLED") + 
                      " (" + robotData.robotMode + ")");
        }
        
        // Parse alliance
        if (doc["alliance"]) {
            robotData.allianceColor = doc["alliance"]["color"] | "unknown";
            robotData.isRedAlliance = doc["alliance"]["isRed"] | false;
            
            if (robotData.allianceColor != "unknown") {
                debugPrint("🔴 Alliance: " + robotData.allianceColor);
            }
        }
        
        // Parse match data
        if (doc["match"]) {
            robotData.matchTimeRemaining = doc["match"]["timeRemaining"] | 0.0;
            debugPrint("🕒 Match time remaining: " + String(robotData.matchTimeRemaining) + "s");
        }
        
        // Parse subsystems
        /*
        if (doc["shooter"]) {
            robotData.shooterReady = doc["shooter"]["ready"] | false;
            robotData.shooterSpeed = doc["shooter"]["speed"] | 0;
            
            if (robotData.shooterReady) {
                debugPrint("🎯 Shooter READY (" + String(robotData.shooterSpeed) + " RPM)");
            }
        }
        
        if (doc["intake"]) {
            robotData.intakeDeployed = doc["intake"]["deployed"] | false;
            robotData.intakePosition = doc["intake"]["position"] | 0.0;
        }
        */

        // Parse auto selection
        if (doc["auto"]) {
            robotData.autoMode = doc["auto"]["selectedMode"] | "Unknown";
            robotData.autoModeNumber = doc["auto"]["modeNumber"] | 0;
        }
        
        // Parse status
        if (doc["status"]) {
            robotData.statusMessage = doc["status"]["message"] | "";
            robotData.heartbeat = doc["status"]["heartbeat"] | 0;
        }

        // Parse vision data
        if (doc["vision"]) {
            robotData.leftCamHasTag = doc["vision"]["leftCam"]["hasTag"] | false;
            robotData.leftCamTagID = doc["vision"]["leftCam"]["tagID"] | -1.0f;
            robotData.rightCamHasTag = doc["vision"]["rightCam"]["hasTag"] | false;
            robotData.rightCamTagID = doc["vision"]["rightCam"]["tagID"] | -1.0f;

            debugPrint("Left Tag: " + String(robotData.leftCamTagID));
            debugPrint("Rght Tag: " + String(robotData.rightCamTagID));
        }
        else{
            debugPrint("no vision data in json");
        }
        
        // Check for significant changes
        if (robotData.hasSignificantChanges(previousRobotData)) {
            hasNewChangedData = true;
            debugPrint("🔄 Significant data change detected!");
        }
    }
    
public:
    TeensyUDPClient(int team, uint16_t port = 5574) : 
        teamNumber(team), 
        localPort(port), 
        robotIP(calculateRobotIP(team)),
        lastPacketTime(0), 
        packetsReceived(0),
        lastDiscoveryTime(0),
        discoveryInterval(5000), // Send discovery every 5 seconds
        robotKnowsAboutUs(false),
        hasNewChangedData(false) {
    }
    
    bool begin() {
        debugPrint("=== Starting UDP Telemetry Client ===");
        debugPrint("Team: " + String(teamNumber));
        debugPrint("Robot IP: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                  String(robotIP[2]) + "." + String(robotIP[3]));
        debugPrint("Listening on port " + String(localPort));
        
        if (udp.begin(localPort)) {
            debugPrint("✅ UDP client started successfully");
            debugPrint("💡 Waiting for robot telemetry packets...");
            return true;
        } else {
            debugPrint("❌ UDP client failed to start");
            return false;
        }
    }
    
    void update() {
        // Handle discovery/heartbeat
        unsigned long now = millis();
        if (now - lastDiscoveryTime >= discoveryInterval) {
            sendDiscoveryMessage();
            lastDiscoveryTime = now;
            
            // Increase interval after initial discovery attempts
            if (!robotKnowsAboutUs && discoveryInterval < 30000) {
                discoveryInterval = min(discoveryInterval + 1000, 30000UL); // Max 30 seconds
            }
        }
        
        // Check for incoming packets
        int packetSize = udp.parsePacket();
        
        if (packetSize > 0) {
            packetsReceived++;
            lastPacketTime = millis();
            robotKnowsAboutUs = true; // Robot responded, so it knows about us
            discoveryInterval = 30000; // Reduce discovery frequency
            
            // Read packet data
            char packetBuffer[1024];
            int bytesRead = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
            packetBuffer[bytesRead] = '\0'; // Null terminate
            
            String jsonData = String(packetBuffer);
            
            debugPrint("📥 Packet #" + String(packetsReceived) + " (" + String(bytesRead) + " bytes)");
            debugPrint("From: " + String(udp.remoteIP()[0]) + "." + String(udp.remoteIP()[1]) + "." + 
                      String(udp.remoteIP()[2]) + "." + String(udp.remoteIP()[3]));
            
            // Parse robot telemetry
            parseRobotData(jsonData);
        }
    }
    
    // Data accessors (same API as NetworkTables client)
    double getBatteryVoltage() const { return robotData.batteryVoltage; }
    bool isRobotEnabled() const { return robotData.robotEnabled; }
    String getRobotMode() const { return robotData.robotMode; }
    bool isShooterReady() const { return robotData.shooterReady; }
    bool isIntakeDeployed() const { return robotData.intakeDeployed; }
    String getAllianceColor() const { return robotData.allianceColor; }
    String getAutoMode() const { return robotData.autoMode; }
    bool isFieldOriented() const { return false; } // Not implemented in UDP version
    double getMatchTimeRemaining() const { return robotData.matchTimeRemaining; }
    String getStatusMessage() const { return robotData.statusMessage; }
    int getHeartbeat() const { return robotData.heartbeat; }
    unsigned long getLastDataUpdate() const { return robotData.lastUpdate; }
    bool getLeftCamHasTag() const { return robotData.leftCamHasTag; }
    float getLeftCamTagID() const { return robotData.leftCamTagID; }    
    bool getRightCamHasTag() const { return robotData.rightCamHasTag; }
    float getRightCamTagID() const { return robotData.rightCamTagID; }

    // UDP-specific status
    bool hasRecentData() const { 
        return (millis() - lastPacketTime) < 2000; // Data within last 2 seconds
    }
    
    /**
     * Returns true if meaningful robot data has changed since last check
     * Ignores timestamps, heartbeats, and other always-changing fields
     * This flag is cleared after being read
     */
    bool hasChangedData() {
        bool result = hasNewChangedData;
        hasNewChangedData = false; // Clear flag after reading
        return result;
    }
    
    int getPacketCount() const { return packetsReceived; }
    
    unsigned long getTimeSinceLastPacket() const {
        if (lastPacketTime == 0) return 999999;
        return millis() - lastPacketTime;
    }
    
    void printStatus() {
        debugPrint("=== UDP Client Status ===");
        debugPrint("Team: " + String(teamNumber));
        debugPrint("Robot IP: " + String(robotIP[0]) + "." + String(robotIP[1]) + "." + 
                  String(robotIP[2]) + "." + String(robotIP[3]));
        debugPrint("Robot knows about us: " + String(robotKnowsAboutUs ? "YES" : "NO"));
        debugPrint("Packets received: " + String(packetsReceived));
        debugPrint("Last packet: " + String(getTimeSinceLastPacket()) + "ms ago");
        debugPrint("Recent data: " + String(hasRecentData() ? "YES" : "NO"));
        debugPrint("Changed data available: " + String(hasNewChangedData ? "YES" : "NO"));

        debugPrint("Left Tag ID" + String(robotData.leftCamTagID));
        debugPrint("Right Tag ID" + String(robotData.rightCamTagID));
        
        if (robotData.lastUpdate > 0) {
            debugPrint("🔋 Battery: " + String(robotData.batteryVoltage) + "V");
            debugPrint("🤖 Robot: " + String(robotData.robotEnabled ? "ENABLED" : "DISABLED"));
            debugPrint("🔴 Alliance: " + robotData.allianceColor);
            debugPrint("🕒 Match time remaining: " + String(robotData.matchTimeRemaining) + "s");
            debugPrint("Data age: " + String(millis() - robotData.lastUpdate) + "ms");
        } else {
            debugPrint("⚠️  No robot data received yet");
        }
        debugPrint("========================");
    }
};

#endif // TEENSY_UDP_CLIENT_H