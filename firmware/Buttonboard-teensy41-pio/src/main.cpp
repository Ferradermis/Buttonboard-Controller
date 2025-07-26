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
uint32_t HSVtoRGB(Adafruit_NeoPixel &strip, uint8_t h, uint8_t s, uint8_t v);
void test_all_pixels();
void testNeoPixels(Adafruit_NeoPixel &strip);


const uint8_t _buttonPins[] = {
  PIN_BTN_01,PIN_BTN_02,PIN_BTN_03,PIN_BTN_04,PIN_BTN_05,PIN_BTN_06,
  PIN_BTN_07,PIN_BTN_08,PIN_BTN_09,PIN_BTN_10,PIN_BTN_11,PIN_BTN_12,
  PIN_BTN_13,PIN_BTN_14,PIN_BTN_15,PIN_BTN_16,PIN_BTN_17,PIN_BTN_18,
  PIN_BTN_19,PIN_BTN_20,PIN_BTN_21,PIN_BTN_22,PIN_BTN_23,PIN_BTN_24,
  PIN_BTN_25,PIN_BTN_26,PIN_BTN_27,PIN_BTN_28
};

const uint8_t numButtons = 23; //sizeof(_buttonPins) / sizeof(_buttonPins[0]);

//arrays to group related buttons
//reef buttons/leds, one will be lit at a time
uint8_t _reefPositionButtons[] = {
  0,1,2,3,4,5,6,7,8,9,10,11
};
//level buttons/leds, one at a time once more
uint8_t _reefLevelButtons[] = {
  12,13,14,15
};


Adafruit_NeoPixel pixels(NUM_LEDS, PIN_NEOPIXELS, NEO_GRB + NEO_KHZ800);

Bounce buttons[numButtons];

// Global objects
FRCNetworkTables nt(TEAM_NUMBER);

uint32_t cReef=pixels.Color(200,0,200);
uint32_t cRed=pixels.Color(200,0,0);
uint32_t cAlgae=pixels.Color(0,200,200);
uint32_t cYellow=pixels.Color(200,200,0);
uint32_t cGreen=pixels.Color(200,200,0);
uint32_t cBlue=pixels.Color(200,200,0);

uint32_t buttonColors[]={
  cReef,cReef,cReef,cReef,cReef,cReef,cReef,cReef,
  cReef,cReef,cReef,cReef,cReef,cReef,cReef,cReef,
  cYellow,cAlgae,cAlgae,cYellow,cYellow,cGreen,cBlue
};


void setup() {
  // put your setup code here, to run once:
  // Initialize each button
    for (uint8_t i = 0; i < numButtons; i++) {
        buttons[i].attach(_buttonPins[i], INPUT_PULLUP);
        buttons[i].interval(10); // 10ms debounce interval
    }

  pixels.begin();
  pixels.clear();
  pixels.show();

  testNeoPixels(pixels);
  //test_all_pixels();

  /*
  Serial.println("Initializing Ethernet...");
  nt.enableDebug();
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
    
    int serialRetries=50;
    while (!Serial && (serialRetries--)>0) {
      delay(10);  // Wait for Serial to be ready
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
  

  // Update button states
  for (uint8_t i = 0; i < numButtons; i++) {
    buttons[i].update();
    if (buttons[i].fell()) {
      //set PIN 13 LED for debug purposes
      digitalWrite(PIN_LED, HIGH);

      //set USB joystick button state
      Joystick.button(i+1,true);

      //set reef buttons to black, then set THIS reef button to purple.
      if (i<12){
        for(int j=0;j<12;j++){
          pixels.setPixelColor(j,0,0,0);
        }
        pixels.setPixelColor(i,200,0,200);
      }

        //set level buttons to black, then set THIS level button to yellow.
      if(i>=12 && i<22){
        for(int j=12;j<16;j++){
          pixels.setPixelColor(j,0,0,0);
        }
        pixels.setPixelColor(i,buttonColors[i]);
      }

      //climb button
      if(i==22){
        for(int j=22;j<23;j++){
          pixels.setPixelColor(j,0,0,0);
        }
        pixels.setPixelColor(i,buttonColors[i]);
      }

      //write led states to leds
      pixels.show();

    }
    if(buttons[i].rose()){
      // If the button is released, turn off the LED
      digitalWrite(PIN_LED, LOW);

      //set the USB joystick button state to released
      Joystick.button(i+1,false);
    }
  }

  

  


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
    pixels.setPixelColor(i,0,255,0);
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


// Helper function to convert HSV to RGB
uint32_t HSVtoRGB(Adafruit_NeoPixel &strip, uint8_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  uint8_t region = h / 43;
  uint8_t remainder = (h - (region * 43)) * 6;
  
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
  switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  
  return strip.Color(r, g, b);
}

void testNeoPixels(Adafruit_NeoPixel &strip) {
  uint16_t numPixels = strip.numPixels();
  uint32_t startTime = millis();
  
  // Phase 1: Rainbow chase (1.5 seconds)
  while (millis() - startTime < 1500) {
    for (int i = 0; i < numPixels; i++) {
      // Create rainbow effect with moving offset
      uint8_t hue = ((i * 255 / numPixels) + (millis() / 10)) & 255;
      strip.setPixelColor(i, HSVtoRGB(strip, hue, 255, 255));
    }
    strip.show();
    delay(20);
  }
  
  // Phase 2: Individual pixel sweep (1.5 seconds)
  startTime = millis();
  while (millis() - startTime < 1500) {
    strip.clear();
    int pos = ((millis() - startTime) * numPixels / 1500) % numPixels;
    
    // Bright white pixel with colorful trail
    strip.setPixelColor(pos, strip.Color(255, 255, 255));
    for (int i = 1; i <= 5 && pos - i >= 0; i++) {
      uint8_t brightness = 255 - (i * 40);
      uint8_t hue = (millis() / 20 + i * 40) & 255;
      uint32_t color = HSVtoRGB(strip, hue, 255, brightness);
      strip.setPixelColor(pos - i, color);
    }
    strip.show();
    delay(30);
  }
  
  // Phase 3: Color fills (1 second)
  uint32_t colors[] = {
    strip.Color(255, 0, 0),   // Red
    strip.Color(0, 255, 0),   // Green  
    strip.Color(0, 0, 255),   // Blue
    strip.Color(255, 255, 0), // Yellow
    strip.Color(255, 0, 255), // Magenta
    strip.Color(0, 255, 255)  // Cyan
  };
  
  for (int c = 0; c < 6; c++) {
    strip.fill(colors[c]);
    strip.show();
    delay(166); // ~1 second total for all colors
  }
  
  // Phase 4: Sparkle effect (1 second)
  startTime = millis();
  while (millis() - startTime < 1000) {
    strip.clear();
    
    // Random sparkles
    for (int i = 0; i < numPixels / 3; i++) {
      int pixel = random(numPixels);
      uint32_t color = HSVtoRGB(strip, random(255), 255, random(128, 255));
      strip.setPixelColor(pixel, color);
    }
    strip.show();
    delay(50);
  }
  
  // Final fade out
  for (int brightness = 255; brightness >= 0; brightness -= 5) {
    strip.fill(strip.Color(brightness / 3, brightness / 3, brightness));
    strip.show();
    delay(20);
  }
  
  strip.clear();
  strip.show();
}