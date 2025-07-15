#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include "FRCNetworkTables.h"

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
#define PIN_NEOPIXELS 37
#define NUM_LEDS 23

#define TEAM_NUMBER 6574

//definition of void test_all_pixels()
void test_all_pixels();


const uint8_t _buttonPins[] = {
  PIN_BTN_01,PIN_BTN_02,PIN_BTN_03,PIN_BTN_04,PIN_BTN_05,PIN_BTN_06,
  PIN_BTN_07,PIN_BTN_08,PIN_BTN_09,PIN_BTN_10,PIN_BTN_11,PIN_BTN_12,
  PIN_BTN_13,PIN_BTN_14,PIN_BTN_15,PIN_BTN_16,PIN_BTN_17,PIN_BTN_18,
  PIN_BTN_19,PIN_BTN_20,PIN_BTN_21,PIN_BTN_22,PIN_BTN_23,PIN_BTN_24,
  PIN_BTN_25,PIN_BTN_26,PIN_BTN_27,PIN_BTN_28
};

const uint8_t numButtons = sizeof(_buttonPins) / sizeof(_buttonPins[0]);

//arrays to group related buttons
//reef buttons, one will be lit at a time
uint8_t _reefPositionButtons[] = {
  0,1,2,3,4,5,6,7,8,9,10,11
};
//level buttons, one at a time once more
uint8_t _reefLevelButtons[] = {
  12,13,14,15
};


Adafruit_NeoPixel pixels(NUM_LEDS, PIN_NEOPIXELS, NEO_GRB + NEO_KHZ800);

Bounce buttons[numButtons];

// Global objects
FRCNetworkTables nt(TEAM_NUMBER);


void setup() {
  // put your setup code here, to run once:
  // Initialize each button
    for (uint8_t i = 0; i < numButtons; i++) {
      pinMode(_buttonPins[i], INPUT_PULLUP); // Set pin mode to INPUT_PULLUP
        buttons[i].attach(_buttonPins[i], INPUT_PULLUP);
        buttons[i].interval(10); // 10ms debounce interval
    }

  pixels.begin();
  pixels.clear();
  pixels.show();

  test_all_pixels();

  /*
  if (nt.begin()) {
        Serial.println("Ethernet initialized successfully");
        Serial.println("Local IP: " + nt.formatIPAddress(Ethernet.localIP()));
        
        // Attempt initial connection
        if (nt.connect()) {
            Serial.println("Connected to robot!");
        } else {
            Serial.println("Initial connection failed - will retry automatically");
        }
    } else {
        Serial.println("Failed to initialize Ethernet!");
    }
*/
    pinMode(PIN_LED, OUTPUT);

    Serial.begin(9600);
    while (!Serial) {
      delay(500);  // Wait for Serial to be ready
    } 
    // Update button states
  for (uint8_t i = 0; i < numButtons; i++) {
    buttons[i].update();
    Serial.print("Button ");
    Serial.print(i + 1);
    Serial.print("State: ");
    Serial.println(buttons[i].read() ? "Released" : "Pressed");
    if (buttons[i].read()) {
      // If the button is pressed, turn on the corresponding LED
      digitalWrite(PIN_LED, HIGH);
    }

  }

}

void loop() {
  // put your main code here, to run repeatedly:

  // Update button states
  for (uint8_t i = 0; i < numButtons; i++) {
    buttons[i].update();
    if (buttons[i].fell()) {
      // If the button is pressed, turn on the corresponding LED
      digitalWrite(PIN_LED, HIGH);
      if (i<12) {
        for(int j=0;j<12;j++)
          pixels.setPixelColor(j,pixels.Color(0, 0, 0));
        pixels.setPixelColor(i, pixels.Color(255, 0, 0)); // Set the pixel to red

      }
    }
    if(buttons[i].rose()){
      // If the button is released, turn off the LED
      digitalWrite(PIN_LED, LOW);
      
    }

  }
  pixels.show();

  


  //nt.update();


  delay(5);
}


void test_all_pixels(){
  int delaytime=20;

  for(int i=0;i<NUM_LEDS;i++){
    pixels.setPixelColor(i,255,0,0);
    pixels.show();
    delay(delaytime);
  }
  for(int i=0;i<NUM_LEDS;i++){
    pixels.setPixelColor(i,0,0,0);
    pixels.show();
    delay(delaytime);
  }

  for(int i=0;i<NUM_LEDS;i++){
    pixels.setPixelColor(i,0,255,0);
    pixels.show();
    delay(delaytime);
  }
  for(int i=0;i<NUM_LEDS;i++){
    pixels.setPixelColor(i,0,0,0);
    pixels.show();
    delay(delaytime);
  }
  for(int i=0;i<NUM_LEDS;i++){
    pixels.setPixelColor(i,0,0,255);
    pixels.show();
    delay(delaytime);
  }

  for(int i=0;i<NUM_LEDS;i++){
    pixels.setPixelColor(i,0,0,0);
    pixels.show();
    delay(delaytime);
  }

  pixels.clear();
  pixels.show();
}


