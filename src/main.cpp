#include <Arduino.h>
#include "credentials.h" // Wifi SSID and PASSWORD
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>        // Built-in
#include "time.h"        // Built-in
#include <SPI.h>         // Built-in
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "epaper_fonts.h"

#include <AsyncMqttClient.h>
#include <HTTPClient.h>

#define SCREEN_WIDTH 300.0 // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 400.0

enum alignment
{
  LEFT,
  RIGHT,
  CENTER
};

// Connections for e.g. LOLIN D32
static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
static const uint8_t EPD_CS = 5;    // to EPD CS
static const uint8_t EPD_RST = 16;  // to EPD RST
static const uint8_t EPD_DC = 17;   // to EPD DC
static const uint8_t EPD_SCK = 18;  // to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23; // to EPD DIN

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts; // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

String time_str, date_str; // strings to hold time and received weather data
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;

// MQTT
AsyncMqttClient mqttClient;

// HTTPClient
WiFiClient client; // or WiFiClientSecure for HTTPS
HTTPClient http;

// MQTT
void InitialiseMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void fetchJson(const char *url);

void BeginSleep();
void DisplayWeather(); // 4.2" e-paper display is 400x300 resolution
void DrawHeadingSection();
void drawString(int x, int y, String text, alignment align);
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align);
void InitialiseDisplay();
uint8_t StartWiFi();
void DisplayData();

// #########################################################################################
void setup()
{
  StartTime = millis();
  Serial.begin(115200);
  if (StartWiFi() == WL_CONNECTED)
  {
    InitialiseDisplay(); // Give screen time to initialise by getting weather data!
    WiFiClient client;   // wifi client object
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(4, 0, "WIFI", LEFT);

    // FetchJson
    fetchJson(http_urls[0]);

    // Initialize the MQTT client
    InitialiseMqtt();

    DisplayData();
    display.display(false); // Full screen update mode
  }
}
// #########################################################################################
void loop()
{
  // this will never run!
}
// #########################################################################################
void DrawHeadingSection()
{
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, GxEPD_BLACK);
}
// #########################################################################################
uint8_t StartWiFi()
{
  Serial.print("\r\nConnecting to: ");
  Serial.println(String(ssid));
  IPAddress dns(192, 168, 22, 6); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection)
  {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000)
    { // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED)
    {
      AttemptConnection = false;
    }
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED)
  {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("WiFi connection *** FAILED ***");
  }
  return connectionStatus;
}

void drawString(int x, int y, String text, alignment align)
{
  char textArray[text.length()+1];
  text.toCharArray(textArray, text.length()+1);
  int16_t textWidth = u8g2Fonts.getUTF8Width(textArray);
  int16_t textAscent = u8g2Fonts.getFontAscent();
  int16_t textDescent = u8g2Fonts.getFontDescent();
  int16_t boxWidth = textWidth + 2;
  int16_t boxHeight = textAscent + textDescent + 2;

  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)
    x = x - w - 10;
  if (align == CENTER)
    x = x - w / 2;

  u8g2Fonts.setCursor(x, y + h + 2); //
  u8g2Fonts.print(text);
}

void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align)
{
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)
    x = x - w;
  if (align == CENTER)
    x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2)
  {
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width)
  {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}

void InitialiseDisplay()
{
  display.init(115200, true, 2, false);
  display.setRotation(3);
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

void DisplayData()
{                       // 4.2" e-paper display is 400x300 resolution
  DrawHeadingSection(); // Top line of the display
}

// MQTT
void InitialiseMqtt()
{
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(SCREEN_WIDTH, 0, "MQTT", RIGHT);
  Serial.print("Connected to MQTT: ");
  for (int i = 0; i < sizeof(mqtt_topics) / sizeof(mqtt_topics[0]); i++)
  {
    Serial.print(mqtt_topics[i]);
    Serial.print(", ");
    mqttClient.subscribe(mqtt_topics[i], 2);
  }
  Serial.println("");
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  // Parse the JSON data
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, len);
  serializeJsonPretty(doc, Serial);
  Serial.println();

  /*float t1 = doc["T1"];
  float t2 = doc["T2"];
  float t3 = doc["T3"];
  String output = "T1: ";
  output += t1;
  output += " T2: ";
  output += t2;
  output += " T3: ";
  output += t3;*/

  bool output = doc["output"];
  String ret = "HEIZUNG: ";
  ret += output;
  Serial.println(ret);

  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(SCREEN_WIDTH / 2, 0, ret, CENTER);
  // display.display(false);
}

// Fetch
void fetchJson(const char *url)
{
  http.useHTTP10(true);
  http.begin(client, url);
  http.GET();

  // Parse response
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, http.getStream());

  // Read values
  /*Serial.println(F("Response:"));
  Serial.println(doc["sensor"].as<const char *>());
  Serial.println(doc["time"].as<long>());
  Serial.println(doc["data"][0].as<float>(), 6);
  Serial.println(doc["data"][1].as<float>(), 6);*/

  bool output = doc["switch:0"]["output"];
  String ret = "HEIZUNG: ";
  ret += output;

  Serial.println(ret);

  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(SCREEN_WIDTH / 2, 0, ret, CENTER);
  // Disconnect
  http.end();
}