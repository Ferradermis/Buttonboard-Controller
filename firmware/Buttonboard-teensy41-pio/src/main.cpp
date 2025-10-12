// Add this to the top of your main.cpp (after other includes)
#include "TeensyUDPClient.h"
#include "NativeEthernet.h"
#include <Bounce2.h>
#include <FastLED.h>
#include <IntervalTimer.h>


#pragma region Button Pins
#define PIN_BTN_01 0
#define PIN_BTN_02 1
#define PIN_BTN_03 2
#define PIN_BTN_04 3
#define PIN_BTN_05 4
#define PIN_BTN_06 5
#define PIN_BTN_07 6
#define PIN_BTN_08 7
#define PIN_BTN_09 8
#define PIN_BTN_10 9
#define PIN_BTN_11 10
#define PIN_BTN_12 11
#define PIN_BTN_13 12
#define PIN_BTN_14 26
#define PIN_BTN_15 27
#define PIN_BTN_16 28
#define PIN_BTN_17 29
#define PIN_BTN_18 30
#define PIN_BTN_19 31
#define PIN_BTN_20 32
#define PIN_BTN_21 33
#define PIN_BTN_22 34
#define PIN_BTN_23 35
#define PIN_BTN_24 36
#define PIN_BTN_25 38
#define PIN_BTN_26 39
#define PIN_BTN_27 40
#define PIN_BTN_28 41
#pragma endregion

#pragma region Joystick Axis Pins
#define PIN_JOY_X 14
#define PIN_JOY_Y 15
#define PIN_JOY_Z 20
#define PIN_JOY_ZR 21
#define PIN_JOY_S1 22
#define PIN_JOY_S2 23
#pragma endregion


#define PIN_LED 13
#define NUM_LEDS 23
#define DATA_PIN 37
#define STATES_PER_LED 4 // Each LED can have 8 animation states (on/off, or different colors)


const uint8_t _buttonPins[] = {
  PIN_BTN_01,PIN_BTN_02,PIN_BTN_03,PIN_BTN_04,PIN_BTN_05,PIN_BTN_06,
  PIN_BTN_07,PIN_BTN_08,PIN_BTN_09,PIN_BTN_10,PIN_BTN_11,PIN_BTN_12,
  PIN_BTN_13,PIN_BTN_14,PIN_BTN_15,PIN_BTN_16,PIN_BTN_17,PIN_BTN_18,
  PIN_BTN_19,PIN_BTN_20,PIN_BTN_21,PIN_BTN_22,PIN_BTN_23,PIN_BTN_24,
  PIN_BTN_25,PIN_BTN_26,PIN_BTN_27,PIN_BTN_28
};

const uint8_t numButtons = 23; //sizeof(_buttonPins) / sizeof(_buttonPins[0]);



Bounce buttons[numButtons];

CRGB leds[NUM_LEDS];
CRGB noopColor= CRGB::Black; // Default noop color
CRGB allianceColor = CRGB::Black; // Default alliance color
CRGB redAllianceColors[STATES_PER_LED]={CRGB::DarkRed, CRGB::Red, CRGB::DarkRed, CRGB::Black};
CRGB blueAllianceColors[STATES_PER_LED]={CRGB::DarkBlue, CRGB::Blue, CRGB::DarkBlue, CRGB::Black};
CRGB algaeColor=CRGB::SeaGreen;



void SetupEthernet();

void SetLEDColors(int ledIndex, std::initializer_list<CRGB> colors);
void SetLEDColor(int ledIndex, CRGB color);
void CheckButtonStates();
void transferColors();
void SetLEDColorFromTagId(int tagId, CRGB color);
void SetLEDColorFromTagIdLeft(int tagId, CRGB color);
void SetLEDColorFromTagIdRight(int tagId, CRGB color);

void isr_animationTimer();
volatile uint8_t animationFrame = 0; // Animation frame counter
IntervalTimer animationTimer; // Timer for animations

CRGB ledStates[NUM_LEDS * STATES_PER_LED];



TeensyUDPClient udpClient(6574);// Your team number


uint32_t LastBatteryCheck=0;

bool foundATag=false;
bool bAutoScoring=false;
bool bAutomated=false;
bool bTeleop=false;
bool bMatchTimeCountingDown=false;
float lastMatchTime=0.0;

// In your setup() function, replace the NetworkTables setup with:
void setup() {
    Serial.begin(115200);
    while (!Serial && (millis() < 5000)) ; // wait for Serial to initialize (with timeout for non-USB serial)
    Serial.println("Buttonboard starting up...");
    
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(192);
    FastLED.clear();
    FastLED.showColor(CRGB::GhostWhite);


    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH); 
    
    SetupEthernet();
    
    delay(2000); // Let ethernet stabilize
    
    if (udpClient.begin()) {
        Serial.println("🎉 UDP client ready!");
    }
    else {
        Serial.println("❌ UDP client failed to start");
    }

    // Initialize the OverlappingLEDManager
    for (int i=0;i<NUM_LEDS * STATES_PER_LED;i++) {
        ledStates[i] = CRGB::Black; // Initialize all states to off
    }
    
    
    animationTimer.begin(isr_animationTimer, 200000); // 500ms interval for animation toggle

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
    udpClient.update();
    // If anything has changed, update the state of the control board.
    if (udpClient.hasChangedData()) {

        foundATag=((udpClient.getLeftCamTagID()>0) || (udpClient.getRightCamTagID()>0));
        if (foundATag) {
            Serial.print("Left Tag ");
            Serial.println(udpClient.getLeftCamTagID());
            Serial.print("Right Tag");
            Serial.println(udpClient.getRightCamTagID());
           if (!bAutoScoring){
            for(int i=0;i<12;i++)
            {
                SetLEDColor(i,CRGB::Black);
            }
                SetLEDColorFromTagIdLeft(udpClient.getLeftCamTagID(),CRGB::Green);
                SetLEDColorFromTagIdRight(udpClient.getRightCamTagID(),CRGB::Green);
           }
           transferColors();
           FastLED.show();
           Serial.println("TAG FOUND");
        }
        else{
            for(int i=0;i<12;i++)
            {
                SetLEDColor(i,CRGB::Black);
            }
            transferColors();
           FastLED.show();
            Serial.println("TAG NOT FOUND");
        }


        if (udpClient.getAllianceColor() =="blue") {
            allianceColor = CRGB::Blue;
        } else if (udpClient.getAllianceColor() =="red") {
            allianceColor = CRGB::Red;
        }
        else{
            allianceColor = CRGB::Black; // Default if no alliance color set
        }

        if (udpClient.getMatchTimeRemaining()==-1.0){
            bMatchTimeCountingDown=false;
        } else if (udpClient.getMatchTimeRemaining()<lastMatchTime){
            bMatchTimeCountingDown=true;
        }
        lastMatchTime=udpClient.getMatchTimeRemaining();

        if (udpClient.getRobotMode()!="Teleop") {
            
                bAutomated=true;
                bMatchTimeCountingDown=false;
        }
        else{
            if (bAutomated){
                bAutomated=false;
                for(int i=0;i<numButtons;i++)
                {
                    SetLEDColor(i, CRGB::Black);
                }
            }

            if(udpClient.getMatchTimeRemaining()<=21.0 && bMatchTimeCountingDown){
                SetLEDColors(22, {CRGB::Red, CRGB::Black, CRGB::Red, CRGB::Black});
            }

        }
        


    }
    
    bAutomated=false;
    if (bAutomated){
        if (udpClient.getAllianceColor() =="blue") {
            for(int i=0;i<numButtons;i++)
            {
                SetLEDColor(i, blueAllianceColors[(animationFrame + i) % STATES_PER_LED]); // Stagger animation by button index
            }
        } else if (udpClient.getAllianceColor() =="red") {
            for(int i=0;i<numButtons;i++)
            {
                SetLEDColor(i, redAllianceColors[(animationFrame + i) % STATES_PER_LED]); // Stagger animation by button index
            }
        }
        else{
            for(int i=0;i<numButtons;i++)
            {
                SetLEDColor(i, CRGB::Black);
            }
        }
    }

    
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
        if(bAutomated){
            //warning=true;
            //break;
        }
      //set PIN 13 LED for debug purposes
      digitalWrite(PIN_LED, HIGH);

      //set USB joystick button state
      Joystick.button(i+1,true);

      //set reef buttons to black, then set THIS reef button to purple.
      if (i<12){
        for(int j=0;j<12;j++){
            SetLEDColors(j, {CRGB::Black, noopColor, CRGB::Black, noopColor});
          
        }
        SetLEDColors(i, {CRGB::Purple, noopColor, CRGB::Purple, noopColor});
      }

      if (i>=12 && i<16){
        for (int j=12;j<16;j++){
            SetLEDColor(j,CRGB::Black);
        }
        SetLEDColor(i,CRGB::Purple);
      }

      
      if (i>=16 && i<22){
        //set level buttons to black, then set THIS level button to purple.
        for(int j=16;j<22;j++){

            SetLEDColor(j, CRGB::Black);
          
        }
        switch(i){
            case 16:
                SetLEDColor(i, CRGB::Blue);
                break;
            case 17:
                SetLEDColor(i, algaeColor);
                break;
            case 18:
                SetLEDColor(i, algaeColor);
                break;
            case 19:
                SetLEDColor(i, CRGB::Yellow);
                break;
            case 20:
                SetLEDColor(i, CRGB::Orange);
                break;
            case 21:
                SetLEDColor(i, CRGB::Purple);
                break;
        }

      }



      
    }
      
    if(buttons[i].rose()){
      // If the button is released, turn off the LED
      digitalWrite(PIN_LED, LOW);

      //set the USB joystick button state to released
      Joystick.button(i+1,false);
    }
  }
    if (warning){
    //if we're automated, just turn off all buttons
            for(int i=0;i<numButtons;i++)
            {
                SetLEDColor(i, CRGB::Orange);
                
            }
            transferColors();
            FastLED.show();
            delay(50);
            for(int i=0;i<numButtons;i++)
            {
                SetLEDColor(i, CRGB::Black);
                
            }
            transferColors();
            FastLED.show();
            delay(50);
            for(int i=0;i<numButtons;i++)
            {
                SetLEDColor(i, CRGB::Orange);
                
            }
            transferColors();
            FastLED.show();
            delay(50);
            for(int i=0;i<numButtons;i++)
            {
                SetLEDColor(i, CRGB::Black);
                
            }
            transferColors();
            FastLED.show();
            
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



void SetLEDColorFromTagIdRight(int tagId, CRGB color){
    switch(tagId){
        case 21:
        case 10:
            SetLEDColor(0,CRGB::Green);
            //SetLEDColor(1,CRGB::Green);
            break;
        case 22:
        case 9:
            SetLEDColor(2,CRGB::Green);
            //SetLEDColor(3,CRGB::Green);
            break;
        case 17:
        case 8:
            SetLEDColor(4,CRGB::Green);
            //SetLEDColor(5,CRGB::Green);
            break;
        case 18:
        case 7:
            SetLEDColor(6,CRGB::Green);
            //SetLEDColor(7,CRGB::Green);
            break;
        case 19:
        case 6:
            SetLEDColor(8,CRGB::Green);
           //SetLEDColor(9,CRGB::Green);
            break;
        case 20:
        case 11:
            SetLEDColor(10,CRGB::Green);
            //SetLEDColor(11,CRGB::Green);
            break;
    }
}

void SetLEDColorFromTagIdLeft(int tagId, CRGB color){
    switch(tagId){
        case 21:
        case 10:
            //SetLEDColor(0,CRGB::Green);
            SetLEDColor(1,CRGB::Green);
            break;
        case 22:
        case 9:
            //SetLEDColor(2,CRGB::Green);
            SetLEDColor(3,CRGB::Green);
            break;
        case 17:
        case 8:
            //SetLEDColor(4,CRGB::Green);
            SetLEDColor(5,CRGB::Green);
            break;
        case 18:
        case 7:
            //SetLEDColor(6,CRGB::Green);
            SetLEDColor(7,CRGB::Green);
            break;
        case 19:
        case 6:
            //SetLEDColor(8,CRGB::Green);
            SetLEDColor(9,CRGB::Green);
            break;
        case 20:
        case 11:
            //SetLEDColor(10,CRGB::Green);
            SetLEDColor(11,CRGB::Green);
            break;
    }
}


void SetupEthernet(){
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
}

void isr_animationTimer(){
    animationFrame = ++animationFrame % STATES_PER_LED; // Toggle frame 0 to STATES_PER_LED-1
}