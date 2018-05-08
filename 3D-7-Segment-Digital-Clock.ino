/*
 * MIT License
 * 
 * Copyright (c) 2018 Leon van den Beukel
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Source: 
 * https://github.com/leonvandenbeukel/3D-7-Segment-Digital-Clock
 * 
 * External libraries you need:
 * Adafruit Sensor Library:   https://github.com/adafruit/Adafruit_Sensor
 * Adafruit RTCLib:           https://github.com/adafruit/RTClib
 * FastLED:                   https://github.com/FastLED/FastLED
 * Timer Library:             https://github.com/JChristensen/Timer
 */

#include <DHT.h>
#include <DHT_U.h>
#include <FastLED.h>
#include <Wire.h>
#include "RTClib.h"
#include <SoftwareSerial.h>
#include "Timer.h"

#define DHTPIN 12
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define NUM_LEDS 30     
#define DATA_PIN 6
CRGB LEDs[NUM_LEDS];

SoftwareSerial BTserial(8, 9);
RTC_DS3231 rtc;
Timer t1;
Timer t2;
Timer t3;

String btBuffer;
CRGB colorCRGB = CRGB::Red;           // Change this if you want another default color, for example CRGB::Blue
CHSV colorCHSV = CHSV(95, 255, 255);  // Green
CRGB colorOFF  = CRGB(20,20,20);      // Color of the segments that are 'disabled'. You can also set it to CRGB::Black
volatile int colorMODE = 1;           // 0=CRGB, 1=CHSV, 2=Constant Color Changing pattern
volatile int mode = 0;                // 0=Clock, 1=Temperature, 2=Humidity, 3=Scoreboard, 4=Time counter
volatile int scoreLeft = 0;
volatile int scoreRight = 0;
volatile long timerValue = 0;
volatile int timerRunning = 0;

#define blinkDots   0                 // Set this to 1 if you want the dots to blink in clock mode, set it to value 0 to disable
#define hourFormat  24                // Set this to 12 or to 24 hour format
#define temperatureMode 'C'           // Set this to 'C' for Celcius or 'F' for Fahrenheit

void setup () {

  // Initialize LED strip
  FastLED.delay(3000);

  // Check if you're LED strip is a RGB or GRB version (third parameter)
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);

  Serial.begin(9600);
  while (!Serial) { /* Wait until serial is ready */ }

  BTserial.begin(9600);
  Serial.println("BTserial started at 9600");

  dht.begin();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
    
  t1.every(1000 * 29, refreshDisplay); 
  t2.every(1000, refreshTimer);
  t3.every(50, updateHue);
  refreshDisplay();
}

void loop () {
  
  t1.update(); 
  t2.update();
  t3.update();

  if (BTserial.available())
  {
    char received = BTserial.read();
    btBuffer += received; 

    if (received == '|')
    {
        processCommand();
        btBuffer = "";
    }
  }
}

void updateHue() {
  if (colorMODE != 2)
    return;
    
  colorCHSV.sat = 255;
  colorCHSV.val = 255;
  if (colorCHSV.hue >= 255){
    colorCHSV.hue = 0;
  } else {
    colorCHSV.hue++;
  }
  refreshDisplay();
}

void refreshDisplay() {
  switch (mode) {
    case 0:
      displayClock();
      break;
    case 1:
      displayTemperature();
      break;
    case 2:
      displayHumidity();
      break;
    case 3:
      displayScoreboard();
      break;      
    case 4:
      // Time counter has it's own timer
      break;
    default:   
      break; 
  }
}

void refreshTimer() {

  if (mode == 0 && blinkDots == 1) {    
    displayDots(3);
  } else if (mode == 4 && timerRunning == 1 && timerValue < 6000) {
    timerValue++;

    int m1 = (timerValue / 60) / 10 ;
    int m2 = (timerValue / 60) % 10 ;
    int s1 = (timerValue % 60) / 10;
    int s2 = (timerValue % 60) % 10;
  
    displaySegments(0, s2); 
    displaySegments(7, s1);
    displaySegments(16, m2);    
    displaySegments(23, m1);  
    displayDots(0);  
    FastLED.show();
  }
}

void processCommand(){

  if (btBuffer.startsWith("RGBD")) {
    long R = getValue(btBuffer, ',', 1).toInt();
    long G = getValue(btBuffer, ',', 2).toInt();
    long B = getValue(btBuffer, ',', 3).toInt();
    long D = getValue(btBuffer, ',', 4).toInt();
    colorCRGB.red = R;
    colorCRGB.green = G;
    colorCRGB.blue = B;
    colorMODE = 0;
    if (D > 0) FastLED.setBrightness(D); 
  } else if (btBuffer.startsWith("HSVD")) {
    long H = getValue(btBuffer, ',', 1).toInt();
    long S = getValue(btBuffer, ',', 2).toInt();
    long V = getValue(btBuffer, ',', 3).toInt();
    long D = getValue(btBuffer, ',', 4).toInt();
    colorCHSV.hue = H;
    colorCHSV.sat = S;
    colorCHSV.val = V;
    colorMODE = 1;
    if (D > 0) FastLED.setBrightness(D);
  } else if (btBuffer.startsWith("RTC")) {
    long y = getValue(btBuffer, ',', 1).toInt();
    long m = getValue(btBuffer, ',', 2).toInt();
    long d = getValue(btBuffer, ',', 3).toInt();
    long h = getValue(btBuffer, ',', 4).toInt();
    long mm = getValue(btBuffer, ',', 5).toInt();
    long s = getValue(btBuffer, ',', 6).toInt();
    rtc.adjust(DateTime(y, m, d, h, mm, s));
    Serial.println("DateTime set");
  } else if (btBuffer.startsWith("CLOCK")) {
    mode = 0;    
  } else if (btBuffer.startsWith("TEMPERATURE")) {
    mode = 1;    
  } else if (btBuffer.startsWith("HUMIDITY")) {
    mode = 2;
  } else if (btBuffer.startsWith("SCOREBOARD")) {
    scoreLeft = getValue(btBuffer, ',', 1).toInt();
    scoreRight = getValue(btBuffer, ',', 2).toInt();
    mode = 3;    
  } else if (btBuffer.startsWith("STARTTIMER")) {
    timerValue = 0;
    timerRunning = 1;
    mode = 4;    
  } else if (btBuffer.startsWith("STOPTIMER")) {
    timerRunning = 0;
    mode = 4;    
  } else if (btBuffer.startsWith("CHANGINGPATTERN")) {
    colorMODE = 2;
  }
  
  refreshDisplay();
}

void displayClock() {   
  DateTime now = rtc.now();

  int h  = now.hour();
  if (hourFormat == 12 && h > 12)
    h = h - 12;
  
  int hl = (h / 10) == 0 ? 13 : (h / 10);
  int hr = h % 10;
  int ml = now.minute() / 10;
  int mr = now.minute() % 10;

  displaySegments(0, mr);    
  displaySegments(7, ml);
  displaySegments(16, hr);    
  displaySegments(23, hl);  
  displayDots(0);  
  FastLED.show();
}

void displayTemperature() {
  float tmp = dht.readTemperature(temperatureMode == 'F' ? true : false);

  if (isnan(tmp)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    int tmp1 = tmp / 10;
    int tmp2 = ((int)tmp) % 10;
    displaySegments(23, tmp1);    
    displaySegments(16, tmp2);
    displaySegments(7,  10);    
    displaySegments(0, (temperatureMode == 'F' ? 14 : 11));
    displayDots(1);  
    FastLED.show();    
  }  
}

void displayHumidity() {
  float hum = dht.readHumidity();

  if (isnan(hum)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    int hum1 = hum / 10;
    int hum2 = ((int)hum) % 10;
    displaySegments(23, hum1);    
    displaySegments(16, hum2);
    displaySegments(7,  10);    
    displaySegments(0,  12);
    displayDots(1);  
    FastLED.show();    
  }  
}

void displayScoreboard() {
  int s1 = scoreLeft % 10;
  int s2 = scoreLeft / 10;
  int s3 = scoreRight % 10;
  int s4 = scoreRight / 10;
  displaySegments(0, s3);    
  displaySegments(7, s4);
  displaySegments(16, s1);    
  displaySegments(23, s2);
  displayDots(2);  
  FastLED.show();  
}

void displayDots(int dotMode) {
  // dotMode: 0=Both on, 1=Both Off, 2=Bottom On, 3=Blink
  switch (dotMode) {
    case 0:
      LEDs[14] = colorMODE == 0 ? colorCRGB : colorCHSV;
      LEDs[15] = colorMODE == 0 ? colorCRGB : colorCHSV; 
      break;
    case 1:
      LEDs[14] = colorOFF;
      LEDs[15] = colorOFF; 
      break;
    case 2:
      LEDs[14] = colorOFF;
      LEDs[15] = colorMODE == 0 ? colorCRGB : colorCHSV; 
      break;
    case 3:
      LEDs[14] = (LEDs[14] == colorOFF) ? (colorMODE == 0 ? colorCRGB : colorCHSV) : colorOFF;
      LEDs[15] = (LEDs[15] == colorOFF) ? (colorMODE == 0 ? colorCRGB : colorCHSV) : colorOFF;
      FastLED.show();  
      break;
    default:
      break;    
  }
}

void displaySegments(int startindex, int number) {

  byte numbers[] = {
    0b00111111, // 0    
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111, // 9   
    0b01100011, // º              10
    0b00111001, // C(elcius)      11
    0b01011100, // º lower        12
    0b00000000, // Empty          13
    0b01110001, // F(ahrenheit)   14
  };

  for (int i = 0; i < 7; i++) {
    LEDs[i + startindex] = ((numbers[number] & 1 << i) == 1 << i) ? (colorMODE == 0 ? colorCRGB : colorCHSV) : colorOFF;
  } 
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
