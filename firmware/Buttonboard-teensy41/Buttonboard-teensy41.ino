#include <Adafruit_NeoPixel.h>

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

#define PIN_JOY_X 14
#define PIN_JOY_Y 15
#define PIN_JOY_Z 20
#define PIN_JOY_ZR 21
#define PIN_JOY_S1 22
#define PIN_JOY_S2 23

#define PIN_GPIO_1 38
#define PIN_GPIO_2 39
#define PIN_GPIO_3 40
#define PIN_GPIO_4 41

#define PIN_LED 13
#define PIN_LEDS 37
#define NUM_LEDS 24

uint8_t _buttonPins[] = {
  PIN_BTN_01,PIN_BTN_02,PIN_BTN_03,PIN_BTN_04,PIN_BTN_05,PIN_BTN_06,
  PIN_BTN_07,PIN_BTN_08,PIN_BTN_09,PIN_BTN_10,PIN_BTN_11,PIN_BTN_12,
  PIN_BTN_13,PIN_BTN_14,PIN_BTN_15,PIN_BTN_16,PIN_BTN_17,PIN_BTN_18,
  PIN_BTN_19,PIN_BTN_20,PIN_BTN_21,PIN_BTN_22,PIN_BTN_23,PIN_BTN_24  
};


Adafruit_NeoPixel pixels(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);


void setup() {
  // put your setup code here, to run once:
  for (int i=0;i<24;i++){
    pinMode(_buttonPins[i],INPUT_PULLUP);
  }

  pixels.begin();
  pixels.clear();
  pixels.show();

}

void loop() {
  // put your main code here, to run repeatedly:

  delay(5);
}
