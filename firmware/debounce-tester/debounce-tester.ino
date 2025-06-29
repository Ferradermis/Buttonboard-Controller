/*
 * Mechanical Switch Bounce Duration Tester
 * 
 * This sketch measures the bounce duration of mechanical switches
 * by capturing all state changes and timing them precisely.
 * 
 * Hardware:
 * - Connect switch between pin 2 and GND
 * - Internal pullup resistor is used
 * - Optional: LED on pin 13 for visual feedback
 * 
 * Usage:
 * 1. Upload sketch and open Serial Monitor (115200 baud)
 * 2. Press and release switch to test
 * 3. View bounce statistics in Serial Monitor
 */

const uint8_t SWITCH_PIN = 2;
const uint8_t LED_PIN = 13;

// Bounce measurement variables
volatile unsigned long lastChangeTime = 0;
volatile bool lastState = HIGH;
volatile bool bounceActive = false;
volatile unsigned long bounceStartTime = 0;
volatile unsigned long bounceEndTime = 0;
volatile uint16_t bounceCount = 0;

// Statistics tracking
unsigned long maxBounceDuration = 0;
unsigned long minBounceDuration = 0xFFFFFFFF;
unsigned long totalBounceDuration = 0;
uint16_t totalBounceEvents = 0;
uint16_t totalTransitions = 0;

// Timing constants
const unsigned long SETTLE_TIME = 50000; // 50ms in microseconds
const unsigned long DEBOUNCE_TIMEOUT = 100000; // 100ms max bounce time

void setup() {
    Serial.begin(115200);
    
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    
    // Enable interrupt on pin change
    attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switchISR, CHANGE);
    
    Serial.println("=== Mechanical Switch Bounce Tester ===");
    Serial.println("Press switch to test bounce characteristics");
    Serial.println("Pin: " + String(SWITCH_PIN) + " (with internal pullup)");
    Serial.println("Format: [Time] Event | Bounce Duration | Statistics");
    Serial.println();
    
    // Initial state
    lastState = digitalRead(SWITCH_PIN);
    digitalWrite(LED_PIN, !lastState); // LED on when switch pressed
}

void loop() {
    // Check for completed bounce events
    if (bounceActive && (micros() - lastChangeTime) > SETTLE_TIME) {
        completeBounceEvent();
    }
    
    // Non-blocking status update every 5 seconds
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 5000 && totalBounceEvents > 0) {
        printStatistics();
        lastStatusUpdate = millis();
    }
    
    delay(1); // Small delay to prevent tight loop
}

void switchISR() {
    unsigned long currentTime = micros();
    bool currentState = digitalRead(SWITCH_PIN);
    
    // Ignore if same state (noise)
    if (currentState == lastState) {
        return;
    }
    
    // Calculate time since last change
    unsigned long timeDiff = currentTime - lastChangeTime;
    
    // Start new bounce event if this is first transition after settle time
    if (!bounceActive && timeDiff > SETTLE_TIME) {
        bounceActive = true;
        bounceStartTime = currentTime;
        bounceCount = 0;
        
        String direction = currentState == LOW ? "PRESS" : "RELEASE";
        Serial.print("[" + String(millis()) + "ms] " + direction + " detected");
    }
    
    // Count transitions during bounce period
    if (bounceActive) {
        bounceCount++;
        bounceEndTime = currentTime;
        
        // Timeout protection
        if (timeDiff > DEBOUNCE_TIMEOUT) {
            Serial.println(" - TIMEOUT!");
            completeBounceEvent();
        }
    }
    
    lastChangeTime = currentTime;
    lastState = currentState;
    totalTransitions++;
    
    // Update LED
    digitalWrite(LED_PIN, !currentState);
}

void completeBounceEvent() {
    if (!bounceActive) return;
    
    bounceActive = false;
    
    unsigned long bounceDuration = bounceEndTime - bounceStartTime;
    
    // Update statistics
    totalBounceEvents++;
    totalBounceDuration += bounceDuration;
    
    if (bounceDuration > maxBounceDuration) {
        maxBounceDuration = bounceDuration;
    }
    if (bounceDuration < minBounceDuration) {
        minBounceDuration = bounceDuration;
    }
    
    // Print bounce results
    Serial.print(" | Bounce: " + String(bounceDuration) + "μs");
    Serial.print(" | Transitions: " + String(bounceCount));
    Serial.print(" | Avg: " + String(totalBounceDuration / totalBounceEvents) + "μs");
    Serial.println();
    
    // Print detailed bounce pattern if significant bouncing occurred
    if (bounceCount > 3) {
        Serial.println("  ⚠ Significant bouncing detected!");
    }
}

void printStatistics() {
    Serial.println();
    Serial.println("=== BOUNCE STATISTICS ===");
    Serial.println("Total bounce events: " + String(totalBounceEvents));
    Serial.println("Total transitions: " + String(totalTransitions));
    Serial.println("Min bounce duration: " + String(minBounceDuration) + "μs (" + 
                   String(minBounceDuration / 1000.0, 1) + "ms)");
    Serial.println("Max bounce duration: " + String(maxBounceDuration) + "μs (" + 
                   String(maxBounceDuration / 1000.0, 1) + "ms)");
    Serial.println("Avg bounce duration: " + String(totalBounceDuration / totalBounceEvents) + "μs (" + 
                   String((totalBounceDuration / totalBounceEvents) / 1000.0, 1) + "ms)");
    Serial.println("Recommended debounce time: " + String((maxBounceDuration / 1000) + 5) + "ms");
    Serial.println("========================");
    Serial.println();
}

// Function to reset statistics (call from Serial if needed)
void resetStatistics() {
    maxBounceDuration = 0;
    minBounceDuration = 0xFFFFFFFF;
    totalBounceDuration = 0;
    totalBounceEvents = 0;
    totalTransitions = 0;
    Serial.println("Statistics reset!");
}