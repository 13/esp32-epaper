#ifndef EPAPER_H
#define EPAPER_H

// Display
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h> // Built-in
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "SFProTextBold32.h"
#include "SFProTextBold55.h"
#include <HTTPClient.h>

#define SCREEN_WIDTH 300.0 // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 400.0

enum alignment
{
  LEFT,
  RIGHT,
  CENTER
};

#if defined(ESP32)
// Wemos S2 mini
static const uint8_t EPD_BUSY = 9;  // to EPD BUSY
static const uint8_t EPD_CS = 10;   // to EPD CS
static const uint8_t EPD_RST = 7;   // to EPD RST
static const uint8_t EPD_DC = 8;    // to EPD DC
static const uint8_t EPD_SCK = 12;  // to EPD CLK
static const uint8_t EPD_MISO = 13; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 11; // to EPD DIN
#endif

extern GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display;
extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

extern WiFiClient wifiClient;
extern HTTPClient httpClient;
extern unsigned long previousMinute;

// Function declarations
void drawSections();
void drawString(int x, int y, String text, alignment align, const uint8_t *font);
void drawStringLine(int x, int y, String text, alignment align, const uint8_t *font);
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align);
void initDisplay();
void displayData();
void fetchJson(const char *url);
void printLocalTime();
void printLocalTime(boolean updateTime);
void loopTime();
void onMqttMessage(char *topic, byte *payload, unsigned int len);

#endif  // EPAPER_H
