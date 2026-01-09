/*
 * OverlappingLEDManager.h
 * 
 * Manages LED strips where individual LEDs can be controlled by multiple
 * systems with different priorities. Higher priority commands override
 * lower priority ones. Perfect for button boards where LEDs need to show
 * both user selection and robot feedback.
 * 
 * Example: Reefscape button board where LEDs show:
 * - Purple for user-selected stalk (Priority 1)
 * - Green flash for April tag detection (Priority 2 - overrides purple)
 */

#ifndef OVERLAPPING_LED_MANAGER_H
#define OVERLAPPING_LED_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>

enum LEDPriority {
    PRIORITY_OFF = 0,
    PRIORITY_SELECTION = 1,     // User selection (e.g., purple for selected stalk)
    PRIORITY_APRIL_TAG = 2,     // Robot feedback (e.g., green flash for April tag)
    PRIORITY_CRITICAL = 3       // Critical alerts (highest priority)
};

struct LEDState {
    CRGB color;
    LEDPriority priority;
    bool isAnimated;
    CRGB animateColor;
    unsigned long lastAnimateTime;
    int animateInterval;
    bool animateState;
    
    LEDState() : 
        color(CRGB::Black), 
        priority(PRIORITY_OFF), 
        isAnimated(false),
        animateColor(CRGB::Black), 
        lastAnimateTime(0), 
        animateInterval(500), 
        animateState(false) {}
};

class OverlappingLEDManager {
private:
    CRGB* leds;
    LEDState* ledStates;
    int numLEDs;
    
public:
    /**
     * Constructor
     * @param ledArray Pointer to FastLED CRGB array
     * @param count Number of LEDs in the array
     */
    OverlappingLEDManager(CRGB* ledArray, int count) : 
        leds(ledArray), numLEDs(count) {
        
        ledStates = new LEDState[numLEDs];
        for (int i = 0; i < numLEDs; i++) {
            clearLED(i);
        }
    }
    
    /**
     * Destructor - clean up allocated memory
     */
    ~OverlappingLEDManager() {
        delete[] ledStates;
    }
    
    /**
     * Set a single LED with priority and optional animation
     * @param index LED index in the array
     * @param color Base color for the LED
     * @param priority Priority level (higher values override lower)
     * @param animate Whether to animate between base and animate colors
     * @param animateColor Color to alternate with (if animating)
     * @param intervalMs Animation interval in milliseconds
     */
    void setLED(int index, CRGB color, LEDPriority priority, 
               bool animate = false, CRGB animateColor = CRGB::Black, int intervalMs = 500) {
        
        if (index < 0 || index >= numLEDs) return;
        
        LEDState& state = ledStates[index];
        
        // Only update if this priority is higher or equal
        if (priority >= state.priority) {
            state.color = color;
            state.priority = priority;
            state.isAnimated = animate;
            state.animateColor = animateColor;
            state.animateInterval = intervalMs;
            state.animateState = false;
            state.lastAnimateTime = millis();
        }
    }
    
    /**
     * Clear LED at specific priority level
     * @param index LED index in the array  
     * @param priority Priority level to clear (PRIORITY_OFF clears all)
     */
    void clearLED(int index, LEDPriority priority = PRIORITY_OFF) {
        if (index < 0 || index >= numLEDs) return;
        
        LEDState& state = ledStates[index];
        
        // Only clear if this matches the current priority or we're clearing all
        if (priority == PRIORITY_OFF || priority == state.priority) {
            state.color = CRGB::Black;
            state.priority = PRIORITY_OFF;
            state.isAnimated = false;
        }
    }
    
    /**
     * Set user selection (mutually exclusive)
     * Clears previous selection and sets new one
     * @param ledIndex Index of LED to select (-1 to clear all)
     * @param color Color for selection (default purple)
     */
    void setSelection(int ledIndex, CRGB color = CRGB::Purple) {
        // Clear all previous selections
        for (int i = 0; i < numLEDs; i++) {
            clearLED(i, PRIORITY_SELECTION);
        }
        
        // Set new selection
        if (ledIndex >= 0 && ledIndex < numLEDs) {
            setLED(ledIndex, color, PRIORITY_SELECTION);
        }
    }
    
    /**
     * Set April tag detection for a pair of LEDs (Reefscape-specific)
     * @param sideIndex Side index (0-5 for 6 sides)
     * @param detected Whether April tag is detected
     * @param ledsPerSide Number of LEDs per side (default 2)
     */
    void setAprilTagDetected(int sideIndex, bool detected, int ledsPerSide = 2) {
        int startLED = sideIndex * ledsPerSide;
        
        for (int i = 0; i < ledsPerSide; i++) {
            int ledIndex = startLED + i;
            if (ledIndex < numLEDs) {
                if (detected) {
                    setLED(ledIndex, CRGB::Green, PRIORITY_APRIL_TAG, 
                          true, CRGB::Black, 300); // Flash green/black every 300ms
                } else {
                    clearLED(ledIndex, PRIORITY_APRIL_TAG);
                }
            }
        }
    }
    
    /**
     * Set LED range with same settings
     * @param startIndex Starting LED index
     * @param count Number of LEDs to set
     * @param color Base color
     * @param priority Priority level
     * @param animate Whether to animate
     * @param animateColor Animation color
     * @param intervalMs Animation interval
     */
    void setLEDRange(int startIndex, int count, CRGB color, LEDPriority priority,
                    bool animate = false, CRGB animateColor = CRGB::Black, int intervalMs = 500) {
        for (int i = 0; i < count; i++) {
            setLED(startIndex + i, color, priority, animate, animateColor, intervalMs);
        }
    }
    
    /**
     * Clear LED range at specific priority
     * @param startIndex Starting LED index
     * @param count Number of LEDs to clear
     * @param priority Priority level to clear
     */
    void clearLEDRange(int startIndex, int count, LEDPriority priority = PRIORITY_OFF) {
        for (int i = 0; i < count; i++) {
            clearLED(startIndex + i, priority);
        }
    }
    
    /**
     * Clear all LEDs at specific priority level
     * @param priority Priority level to clear (PRIORITY_OFF clears everything)
     */
    void clearAll(LEDPriority priority = PRIORITY_OFF) {
        for (int i = 0; i < numLEDs; i++) {
            clearLED(i, priority);
        }
    }
    
    /**
     * Main update loop - call this every loop() iteration
     * Handles animations and updates physical LEDs via FastLED.show()
     */
    void update() {
        unsigned long now = millis();
        
        for (int i = 0; i < numLEDs; i++) {
            LEDState& state = ledStates[i];
            
            CRGB currentColor = state.color;
            
            // Handle animation
            if (state.isAnimated && state.priority > PRIORITY_OFF) {
                if (now - state.lastAnimateTime >= state.animateInterval) {
                    state.animateState = !state.animateState;
                    state.lastAnimateTime = now;
                }
                
                if (state.animateState) {
                    currentColor = state.animateColor;
                }
            }
            
            leds[i] = currentColor;
        }
        
        FastLED.show();
    }
    
    /**
     * Get current priority of an LED
     * @param index LED index
     * @return Current priority level
     */
    LEDPriority getLEDPriority(int index) const {
        if (index < 0 || index >= numLEDs) return PRIORITY_OFF;
        return ledStates[index].priority;
    }
    
    /**
     * Check if LED is currently animating
     * @param index LED index
     * @return True if LED is animating
     */
    bool isLEDAnimating(int index) const {
        if (index < 0 || index >= numLEDs) return false;
        return ledStates[index].isAnimated && ledStates[index].priority > PRIORITY_OFF;
    }
    
    /**
     * Get number of LEDs managed
     * @return Number of LEDs
     */
    int getLEDCount() const {
        return numLEDs;
    }
    
    /**
     * Debug helper - print LED status to Serial
     */
    void printStatus() const {
        Serial.println("=== LED Manager Status ===");
        for (int i = 0; i < numLEDs; i++) {
            const LEDState& state = ledStates[i];
            Serial.printf("LED %2d: Priority %d, Color RGB(%3d,%3d,%3d), Animated %s\n", 
                         i, state.priority, 
                         state.color.r, state.color.g, state.color.b,
                         state.isAnimated ? "YES" : "NO");
        }
        Serial.println("=========================");
    }
};

#endif // OVERLAPPING_LED_MANAGER_H