/**
 * Example code to make the Saintcon 2025 Nut into a BLE keyboard that controls volume and play/pause
 * Needs the following:
 * Boards Manager: esp32 by Espressif Systems (v3.2.0 used)
 * Board Selected: LOLIN C3 Mini
 * Libraries: Adafruit NeoPixel by Adafruit (v1.12.5 used)
 * BLE stuff based on the BleKeyboard library which is included with the sketch (because it needs some changes that can be found in the pull requests)
 *  More info about the library: https://github.com/T-vK/ESP32-BLE-Keyboard
 */
#include <BleKeyboard.h>
#include <Adafruit_NeoPixel.h>

#define CWPIN 1
#define CCWPIN 0
#define BUTTONPIN 21

#define LEDPIN 10
#define NUMPIXELS 6

Adafruit_NeoPixel pixels(NUMPIXELS, LEDPIN, NEO_GRB + NEO_KHZ800);

BleKeyboard bleKeyboard("SC25Nut", "MK Factor", 100);

volatile bool volup = false;
volatile bool voldown = false;
volatile bool buttonpress = false;

int led_counter = 0;
int led_off_delay = 500;
unsigned long led_on_time = 0;

void setup() {
  pixels.begin();
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleKeyboard.begin();
  pinMode(CWPIN, INPUT_PULLUP);
  pinMode(CCWPIN, INPUT_PULLUP);
  attachInterrupt(CWPIN, CWInterrupt, FALLING);
  attachInterrupt(CCWPIN, CCWInterrupt, FALLING);
  pinMode(BUTTONPIN, INPUT_PULLUP);
  attachInterrupt(BUTTONPIN, BUTTONInterrupt, FALLING);
}

void loop() {
  if(bleKeyboard.isConnected()) {
    if(volup){
      
      led_on_time = millis();
      pixels.clear();
      led_counter ++;
      if(led_counter > NUMPIXELS-1) led_counter = 0;
      pixels.setPixelColor(led_counter, pixels.Color(255,255,255));
      pixels.show();
      bleKeyboard.press(KEY_MEDIA_VOLUME_UP);
      delay(10);
      bleKeyboard.releaseAll();
      volup = false;
    }
    if(voldown){
      
      led_on_time = millis();
      pixels.clear();
      led_counter --;
      if(led_counter < 0) led_counter = NUMPIXELS-1;
      pixels.setPixelColor(led_counter, pixels.Color(255,255,255));
      pixels.show();
      bleKeyboard.press(KEY_MEDIA_VOLUME_DOWN);
      delay(10);
      bleKeyboard.releaseAll();
      voldown = false;
    }
    if(buttonpress){
      led_on_time = millis();
      pixels.clear();
      for(int x=0; x<NUMPIXELS; x++){
        pixels.setPixelColor(x, pixels.Color(255,255,0));
      }
      pixels.show();
      bleKeyboard.press(KEY_MEDIA_PLAY_PAUSE);
      delay(10);
      bleKeyboard.releaseAll();
      delay(250);
      buttonpress = false;
    }
    if(millis() - led_on_time > led_off_delay){
      led_on_time = millis(); //so that we don't try to clear the pixels every time through the loop
      pixels.clear();
      pixels.show();
    }
  }
}

void CWInterrupt(){
  volup = true;
}

void CCWInterrupt(){
  voldown = true;
}

void BUTTONInterrupt(){
  buttonpress = true;
}