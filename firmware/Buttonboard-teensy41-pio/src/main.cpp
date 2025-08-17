/*
 * Simple connection test to replace your main.cpp setup temporarily
 * This will help us isolate the connection issue
 */

// Add this to the top of your main.cpp (after other includes)
#include "TeensyUDPClient.h"
#include "NativeEthernet.h"

// Replace your global RobustNTClient with this:
TeensyUDPClient udpClient(6574);// Your team number


uint32_t LastBatteryCheck=0;

// In your setup() function, replace the NetworkTables setup with:
void setup() {
    Serial.begin(9600);
    
    // ... your existing button and LED setup code ...
    
    // Simple ethernet setup
    byte mac[6] = {0x02, 0xFE, 0xED, 0x65, 0x74, 0x64};
    Serial.println("Starting Ethernet...");
    
    if (Ethernet.begin(mac,15000,5000)) {
        Serial.println("✅ Ethernet configured via DHCP");
    } else {
        // Try static IP
        IPAddress ip(10, 65, 74, 100);  // Team 6574 static IP
        IPAddress gateway(10, 65, 74, 1);
        IPAddress subnet(255, 255, 255, 0);
        Ethernet.begin(mac, ip, gateway, gateway, subnet);
        Serial.println("✅ Ethernet configured with static IP");
    }
    
    Serial.print("Local IP: ");
    Serial.println(Ethernet.localIP());
    
    delay(2000); // Let ethernet stabilize
    
    if (udpClient.begin()) {
        Serial.println("🎉 UDP client ready!");
    }

    


}

// In your loop() function, replace the NetworkTables update with:
void loop() {
    udpClient.update();
    
    // Same API as before!
    if (udpClient.hasRecentData()) {
        Serial.println("Battery: " + String(udpClient.getBatteryVoltage()) + "V");
    }
    
    delay(5);
}