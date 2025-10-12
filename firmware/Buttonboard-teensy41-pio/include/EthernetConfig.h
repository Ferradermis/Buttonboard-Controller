/*
 * Ethernet Configuration for Teensy 4.1 with DHCP and Static Fallback
 * Designed for FRC competition networks
 */

#ifndef ETHERNET_CONFIG_H
#define ETHERNET_CONFIG_H

#include <Arduino.h>
#include <NativeEthernet.h>

class EthernetConfig {
private:
    int teamNumber;
    byte mac[6];
    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns;
    bool isConnected;
    bool usingDHCP;
    unsigned long dhcpTimeout;
    unsigned long lastConnectionCheck;
    
    void generateTeamMAC();
    IPAddress calculateTeamIP(int team, int deviceNumber = 100);
    bool tryDHCP();
    bool tryStatic();
    void printNetworkConfig();

public:
    EthernetConfig(int teamNumber);
    bool begin();
    bool begin(int deviceNumber); // For multiple control boards
    void update(); // Call periodically to maintain connection
    bool isConnectionActive();
    IPAddress getLocalIP();
    IPAddress getRobotIP();
    void printStatus();
    
    // Configuration options
    void setDHCPTimeout(unsigned long timeout) { dhcpTimeout = timeout; }
    void forceStatic(); // Skip DHCP and go straight to static
};

EthernetConfig::EthernetConfig(int teamNumber) :
    teamNumber(teamNumber),
    isConnected(false),
    usingDHCP(false),
    dhcpTimeout(10000), // 10 second DHCP timeout
    lastConnectionCheck(0)
{
    // Generate team-specific MAC address
    generateTeamMAC();
    
    // Calculate team-specific network configuration
    staticIP = calculateTeamIP(teamNumber, 100);
    gateway = calculateTeamIP(teamNumber, 1);  // Router is always .1
    subnet = IPAddress(255, 255, 255, 0);
    dns = gateway; // Use router as DNS
}

bool EthernetConfig::begin() {
    return begin(100); // Default device number
}

bool EthernetConfig::begin(int deviceNumber) {
    Serial.println("=== Ethernet Configuration Starting ===");
    Serial.printf("Team Number: %d\n", teamNumber);
    Serial.printf("Device Number: %d\n", deviceNumber);
    
    // Update static IP for this device number
    staticIP = calculateTeamIP(teamNumber, deviceNumber);
    
    // Print MAC address
    Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Try DHCP first
    Serial.println("Attempting DHCP configuration...");
    if (tryDHCP()) {
        usingDHCP = true;
        isConnected = true;
        Serial.println("✓ DHCP configuration successful");
        printNetworkConfig();
        return true;
    }
    
    // DHCP failed, try static
    Serial.println("DHCP failed, attempting static configuration...");
    if (tryStatic()) {
        usingDHCP = false;
        isConnected = true;
        Serial.println("✓ Static configuration successful");
        printNetworkConfig();
        return true;
    }
    
    // Both failed
    Serial.println("✗ Both DHCP and static configuration failed");
    isConnected = false;
    return false;
}

void EthernetConfig::forceStatic() {
    Serial.println("Forcing static IP configuration...");
    if (tryStatic()) {
        usingDHCP = false;
        isConnected = true;
        Serial.println("✓ Static configuration successful");
        printNetworkConfig();
    } else {
        Serial.println("✗ Static configuration failed");
        isConnected = false;
    }
}

bool EthernetConfig::tryDHCP() {
    unsigned long startTime = millis();
    
    // Try DHCP with timeout
    int result = Ethernet.begin(mac, dhcpTimeout);
    
    if (result == 1) {
        // DHCP succeeded
        Serial.printf("DHCP assigned IP: %d.%d.%d.%d\n", 
                     Ethernet.localIP()[0], Ethernet.localIP()[1], 
                     Ethernet.localIP()[2], Ethernet.localIP()[3]);
        return true;
    } else {
        // DHCP failed
        Serial.printf("DHCP failed after %lu ms\n", millis() - startTime);
        return false;
    }
}

bool EthernetConfig::tryStatic() {
    // Configure static IP
    Ethernet.begin(mac, staticIP, dns, gateway, subnet);
    
    // Give it a moment to initialize
    delay(1000);
    
    // Check if Ethernet link is up
    if (Ethernet.linkStatus() == LinkON) {
        Serial.printf("Static IP configured: %d.%d.%d.%d\n", 
                     staticIP[0], staticIP[1], staticIP[2], staticIP[3]);
        return true;
    } else {
        Serial.println("No Ethernet link detected");
        return false;
    }
}

void EthernetConfig::update() {
    unsigned long currentTime = millis();
    
    // Check connection status every 5 seconds
    if (currentTime - lastConnectionCheck >= 5000) {
        lastConnectionCheck = currentTime;
        
        // Check link status
        auto linkStatus = Ethernet.linkStatus();
        bool linkUp = (linkStatus == LinkON);
        
        if (!linkUp && isConnected) {
            // Link went down
            Serial.println("Ethernet link lost");
            isConnected = false;
        } else if (linkUp && !isConnected) {
            // Link came back up
            Serial.println("Ethernet link restored");
            
            if (usingDHCP) {
                // Try to renew DHCP lease
                Serial.println("Attempting DHCP renewal...");
                int maintainResult = Ethernet.maintain();
                if (maintainResult == 1 || maintainResult == 3) {
                    Serial.println("DHCP lease renewed");
                    isConnected = true;
                } else {
                    Serial.println("DHCP renewal failed, switching to static");
                    if (tryStatic()) {
                        usingDHCP = false;
                        isConnected = true;
                    }
                }
            } else {
                // Static IP should work immediately when link comes back
                isConnected = true;
            }
        }
        
        // If using DHCP, periodically maintain the lease
        if (isConnected && usingDHCP) {
            int maintainResult = Ethernet.maintain();
            if (maintainResult != 0) {
                Serial.printf("DHCP maintain result: %d\n", maintainResult);
            }
        }
    }
}

bool EthernetConfig::isConnectionActive() {
    return isConnected && (Ethernet.linkStatus() == LinkON);
}

IPAddress EthernetConfig::getLocalIP() {
    if (usingDHCP) {
        return Ethernet.localIP();
    } else {
        return staticIP;
    }
}

IPAddress EthernetConfig::getRobotIP() {
    return calculateTeamIP(teamNumber, 2); // Robot is always .2
}

void EthernetConfig::generateTeamMAC() {
    // Generate a team-specific MAC address
    // Format: 02:xx:xx:TT:TT:DD where TTTT is team number, DD is device
    mac[0] = 0x02; // Locally administered MAC
    mac[1] = 0xFE; // Fixed identifier
    mac[2] = 0xED; // Fixed identifier  
    mac[3] = (teamNumber >> 8) & 0xFF;  // Team number high byte
    mac[4] = teamNumber & 0xFF;         // Team number low byte
    mac[5] = 0x64; // Device identifier (100 decimal = 0x64)
}

IPAddress EthernetConfig::calculateTeamIP(int team, int deviceNumber) {
    if (team <= 0) {
        // Default network for testing
        return IPAddress(10, 0, 0, deviceNumber);
    }
    
    // FRC standard: 10.TE.AM.XXX
    int firstOctet = team / 100;
    int secondOctet = team % 100;
    return IPAddress(10, firstOctet, secondOctet, deviceNumber);
}

void EthernetConfig::printNetworkConfig() {
    IPAddress localIP = getLocalIP();
    IPAddress robotIP = getRobotIP();
    
    Serial.println("--- Network Configuration ---");
    Serial.printf("Local IP: %d.%d.%d.%d\n", localIP[0], localIP[1], localIP[2], localIP[3]);
    Serial.printf("Gateway: %d.%d.%d.%d\n", gateway[0], gateway[1], gateway[2], gateway[3]);
    Serial.printf("Subnet: %d.%d.%d.%d\n", subnet[0], subnet[1], subnet[2], subnet[3]);
    Serial.printf("DNS: %d.%d.%d.%d\n", dns[0], dns[1], dns[2], dns[3]);
    Serial.printf("Robot IP: %d.%d.%d.%d\n", robotIP[0], robotIP[1], robotIP[2], robotIP[3]);
    Serial.printf("Method: %s\n", usingDHCP ? "DHCP" : "Static");
    Serial.println("-----------------------------");
}

void EthernetConfig::printStatus() {
    Serial.println("=== Ethernet Status ===");
    Serial.printf("Connected: %s\n", isConnected ? "YES" : "NO");
    Serial.printf("Link Status: %s\n", (Ethernet.linkStatus() == LinkON) ? "UP" : "DOWN");
    Serial.printf("Method: %s\n", usingDHCP ? "DHCP" : "Static");
    
    if (isConnected) {
        IPAddress localIP = getLocalIP();
        Serial.printf("IP Address: %d.%d.%d.%d\n", localIP[0], localIP[1], localIP[2], localIP[3]);
    }
    
    Serial.println("======================");
}

#endif // ETHERNET_CONFIG_H