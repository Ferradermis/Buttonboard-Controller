// Add this to the top of your main.cpp (after other includes)
#include <Bounce2.h>
#include <FastLED.h>
#include <IntervalTimer.h>
#include <LiquidCrystal_I2C.h>
#include "PinAssignments.h"



#define PIN_LED 13
#define NUM_LEDS 6

#define STATES_PER_LED 4 // Each LED can have 8 animation states (on/off, or different colors)


const uint8_t _buttonPins[] = {
  PIN_BTN_01,PIN_BTN_02,PIN_BTN_03,PIN_BTN_04,PIN_BTN_05,PIN_BTN_06
};

const uint8_t numButtons = 6; //sizeof(_buttonPins) / sizeof(_buttonPins[0]);



Bounce buttons[numButtons];

CRGB leds[NUM_LEDS];
CRGB noopColor= CRGB::Black; // Default noop color
CRGB allianceColor = CRGB::Black; // Default alliance color
CRGB redAllianceColors[STATES_PER_LED]={CRGB::DarkRed, CRGB::Red, CRGB::DarkRed, CRGB::Black};
CRGB blueAllianceColors[STATES_PER_LED]={CRGB::DarkBlue, CRGB::Blue, CRGB::DarkBlue, CRGB::Black};
CRGB algaeColor=CRGB::SeaGreen;




void SetLEDColors(int ledIndex, std::initializer_list<CRGB> colors);
void SetLEDColor(int ledIndex, CRGB color);
void CheckButtonStates();
void transferColors();

// LED test helper (declared here for PlatformIO builds)
void RunLEDTestAnimation();

void isr_animationTimer();
volatile uint8_t animationFrame = 0; // Animation frame counter
IntervalTimer animationTimer; // Timer for animations

CRGB ledStates[NUM_LEDS * STATES_PER_LED];



//TeensyUDPClient udpClient(6574);// Your team number


uint32_t LastBatteryCheck=0;

bool foundATag=false;
bool bAutomated=false;
bool bMatchTimeCountingDown=false;
float lastMatchTime=0.0;

LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD at I2C address 0x27, 16 chars, 2 lines


// In your setup() function, replace the NetworkTables setup with:
void setup() {
    Serial.begin(115200);
    while (!Serial && (millis() < 5000)) ; // wait for Serial to initialize (with timeout for non-USB serial)
    
    Serial.println("Buttonboard starting up...");
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Starting...");
    
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(224);
    FastLED.clear();
    // Run a short LED test animation to visually verify LEDs
    RunLEDTestAnimation();


    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH); 
    
    //SetupEthernet();
    
    delay(2000); // Let ethernet stabilize
    
    // if (udpClient.begin()) {
    //     Serial.println("🎉 UDP client ready!");
    // }
    // else {
    //     Serial.println("❌ UDP client failed to start");
    // }

    // Initialize the OverlappingLEDManager
    for (int i=0;i<NUM_LEDS * STATES_PER_LED;i++) {
        ledStates[i] = CRGB::Black; // Initialize all states to off
    }
    
    
    animationTimer.begin(isr_animationTimer, 200000); // 200ms interval for animation toggle

    for (int i=0;i<numButtons;i++) {
        buttons[i].attach(_buttonPins[i], INPUT_PULLUP);
        buttons[i].interval(50); // 50ms debounce interval
    }

    //yeah this is our throw-away color.  The SetLEDColors function will ignore this color and leave the LED unchanged.
    noopColor.setRGB(1,2,3); 
    Joystick.X(512);
    Joystick.Y(512);   
    Joystick.Z(512);
    Joystick.Zrotate(512);
    Joystick.slider(512);
    Joystick.sliderLeft(512);
    Joystick.sliderRight(512);

    digitalWrite(PIN_LED, LOW);
}


void loop() {

    //check for messages from robot
    // udpClient.update();
    // // If anything has changed, update the state of the control board.
    // if (udpClient.hasChangedData()) {

        


    //     if (udpClient.getAllianceColor() =="blue") {
    //         allianceColor = CRGB::Blue;
    //     } else if (udpClient.getAllianceColor() =="red") {
    //         allianceColor = CRGB::Red;
    //     }
    //     else{
    //         allianceColor = CRGB::Black; // Default if no alliance color set
    //     }

    //     if (udpClient.getMatchTimeRemaining()==-1.0){
    //         bMatchTimeCountingDown=false;
    //     } else if (udpClient.getMatchTimeRemaining()<lastMatchTime){
    //         bMatchTimeCountingDown=true;
    //     }
    //     lastMatchTime=udpClient.getMatchTimeRemaining();

    //     if (udpClient.getRobotMode()!="Teleop") {
            
    //             bAutomated=true;
    //             bMatchTimeCountingDown=false;
    //     }
    //     else{
    //         if (bAutomated){
    //             bAutomated=false;
    //             for(int i=0;i<numButtons;i++)
    //             {
    //                 SetLEDColor(i, CRGB::Black);
    //             }
    //         }

    //         if(udpClient.getMatchTimeRemaining()<=21.0 && bMatchTimeCountingDown){
    //             SetLEDColors(22, {CRGB::Red, CRGB::Black, CRGB::Red, CRGB::Black});
    //         }

    //     }
        


    // }
    
    // //bAutomated=false;
    // if (bAutomated){
    //     if (udpClient.getAllianceColor() =="blue") {
    //         for(int i=0;i<numButtons;i++)
    //         {
    //             SetLEDColor(i, blueAllianceColors[(animationFrame + i) % STATES_PER_LED]); // Stagger animation by button index
    //         }
    //     } else if (udpClient.getAllianceColor() =="red") {
    //         for(int i=0;i<numButtons;i++)
    //         {
    //             SetLEDColor(i, redAllianceColors[(animationFrame + i) % STATES_PER_LED]); // Stagger animation by button index
    //         }
    //     }
    //     else{
    //         for(int i=0;i<numButtons;i++)
    //         {
    //             SetLEDColor(i, CRGB::Black);
    //         }
    //     }
    // }

    
    CheckButtonStates();
    
            


    //update the LED colors according to current animation frame

    transferColors();

    FastLED.show();

    delay(0);
}

void transferColors(){
    noInterrupts();
    for (int i = 0; i < NUM_LEDS; i++) {
        // Update each LED based on its current state
        CRGB currentColor = ledStates[i * STATES_PER_LED + animationFrame];
        FastLED.leds()[i] = currentColor;
    }
    interrupts();
}

void CheckButtonStates() {
    bool warning=false;
    for (uint8_t i = 0; i < numButtons; i++) {
    buttons[i].update();
    if (buttons[i].fell()) {
        // If the button is released, turn off the LED
      digitalWrite(PIN_LED, HIGH);

      SetLEDColor(i, CRGB::Green);
      //set the USB joystick button state to released
      Joystick.button(i+1,true);
    }
      
    if(buttons[i].rose()){
      // If the button is released, turn off the LED
      digitalWrite(PIN_LED, LOW);
        SetLEDColor(i, CRGB::Black);

      //set the USB joystick button state to released
      Joystick.button(i+1,false);
    }
  }
    
}





void SetLEDColors(int ledIndex, std::initializer_list<CRGB> colors) {
    if (ledIndex < 0 || ledIndex >= NUM_LEDS) return; // Out of bounds
    
    int i = 0;
    for (const CRGB& color : colors) {
        if (i >= STATES_PER_LED) break; // Don't exceed array bounds
        if (color == noopColor) {
            // If color is noop, ignore
        } else {
            // Otherwise, set to the specified color
            ledStates[ledIndex * STATES_PER_LED + i] = color;
        }
        i++;
    }
    
}

void SetLEDColor(int ledIndex, CRGB color) {
    if (ledIndex < 0 || ledIndex >= NUM_LEDS) return; // Out of bounds
    for (int i = 0; i < STATES_PER_LED; i++) {
        ledStates[ledIndex * STATES_PER_LED + i] = color;
    }
    
}


void isr_animationTimer(){
    animationFrame = ++animationFrame % STATES_PER_LED; // Toggle frame 0 to STATES_PER_LED-1
}

// A short, visually pleasing LED test sequence to verify FastLED wiring and LEDs.
// Total duration is approximately 4000 ms (4 seconds).
void RunLEDTestAnimation() {
    const unsigned long totalMs = 4000;
    unsigned long startMs = millis();
    while (millis() - startMs < totalMs) {
        unsigned long t = millis() - startMs;

        if (t < 1000) {
            // 0..1000ms : smooth rainbow sweep
            uint8_t hue = map((int)t, 0, 1000, 0, 255);
            fill_rainbow(leds, NUM_LEDS, hue, 7);
            FastLED.show();
            delay(20);
        }
        else if (t < 2200) {
            // 1000..2200ms : theater chase rainbow (~1200ms)
            unsigned long tt = t - 1000;
            int phase = (tt / 120) % 3;
            uint8_t hue = (tt / 5) & 0xFF;
            for (int i = 0; i < NUM_LEDS; i++) {
                if ((i % 3) == phase) leds[i] = CHSV((hue + i * 8) & 0xFF, 200, 255);
                else leds[i] = CRGB::Black;
            }
            FastLED.show();
            delay(40);
        }
        else if (t < 3200) {
            // 2200..3200ms : soft teal pulse in/out (~1000ms)
            unsigned long tt = t - 2200;
            uint8_t b;
            if (tt < 500) b = map((int)tt, 0, 500, 0, 255);
            else b = map((int)(tt - 500), 0, 500, 255, 0);
            fill_solid(leds, NUM_LEDS, CHSV(160, 200, b));
            FastLED.show();
            delay(12);
        }
        else {
            // 3200..4000ms : final white sweep (~800ms)
            unsigned long tt = t - 3200;
            int idx = map((int)tt, 0, 800, 0, NUM_LEDS);
            for (int i = 0; i < NUM_LEDS; i++) {
                leds[i] = (i <= idx) ? CRGB::White : CRGB::Black;
            }
            FastLED.show();
            delay(30);
        }
    }

    FastLED.clear();
    FastLED.show();
}