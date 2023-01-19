// #include "my_credentials.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2_for_Adafruit_GFX.h>

#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// MQTT server credentials
#define MQTT_HOST IPAddress(192, 168, 22, 5)
#define MQTT_PORT 1883
#define MQTT_TOPIC "shellies/HZ_DG/status/switch:0"

// URL
#define URL "http://192.168.22.70/rpc/Shelly.GetStatus"

#define SCREEN_WIDTH  400.0    // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 300.0

enum alignment {LEFT, RIGHT, CENTER};

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

AsyncMqttClient mqttClient;
// HTTPClient
WiFiClient client; // or WiFiClientSecure for HTTPS
HTTPClient http;

// Initialize the e-paper display
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=*/5, /*DC=*/17, /*RST=*/16, /*BUSY=*/4)); // GDEW042T2 400x300, UC8176 (IL0398)

void InitialiseDisplay();
void displayFull(String text);
void displayPartial(String text);
void onMqttConnect(bool sessionPresent);
void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void fetchJson(const char *url);
void DrawHeadingSection();
void drawString(int x, int y, String text, alignment align);
void DisplayData();

void setup()
{
  Serial.begin(115200);
  delay(10);
  delay(4000);
  // boot
  Serial.printf("\n\nBooting... Compiled: %s\n", __TIMESTAMP__);
  String bootText = "Booting...\nCompiled: ";
  bootText += __TIMESTAMP__;

  // display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  /*displayFull(bootText);
  delay(1000);
  display.powerOff();*/
  InitialiseDisplay();


  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  // displayPartial("WIFI");
  drawString(4, 0, "WIFI", LEFT);

  // FetchJson
  fetchJson(URL);

  // Initialize the MQTT client
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.connect();
}

void loop()
{
  // nothing to do here
}

void displayFull(String text)
{
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104)
    display.setFont(0);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  Serial.printf("\nwidth: %d height: %d\n", display.width(), display.height());
  Serial.printf("\ntbx: %d tby: %d tbw: %d tbh: %d\n", tbx, tby, tbw, tbh);
  // center bounding box by transposition of origin:
  // uint16_t x = ((display.width() - tbw) / 2) - tbx;
  // uint16_t y = ((display.height() - tbh) / 2) - tby;
  // bottom
  // uint16_t x = ((display.width() - tbw)) - tbx;
  // uint16_t y = ((display.height() - tbh)) - tby;
  // top left
  // uint16_t x = display.width();
  // uint16_t y = 0;
  // top right
  // uint16_t x = (display.width() - tbw) -tbx;
  // uint16_t y = 0 + tbh;
  // bottom left
  uint16_t x = 0;
  uint16_t y = display.height();
  // bottom right
  // uint16_t x = (display.width() - tbw) -tbx;
  // uint16_t y = (display.height() - tbh) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(text);
  } while (display.nextPage());
  Serial.println(text);
}

void displayPartial(String text)
{
  Serial.println(text);
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  // do this outside of the loop
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  // center update text
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t utx = ((display.width() - tbw) / 2) - tbx;
  uint16_t uty = ((display.height() / 4) - tbh / 2) - tby;
  // center update mode
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t umx = ((display.width() - tbw) / 2) - tbx;
  uint16_t umy = ((display.height() * 3 / 4) - tbh / 2) - tby;
  // center HelloWorld
  // display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t hwx = ((display.width() - tbw) / 2) - tbx;
  uint16_t hwy = ((display.height() - tbh) / 2) - tby;
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    // display.setCursor(hwx, hwy);
    // display.print(HelloWorld);
    display.drawFastHLine(0,16,128,GxEPD_BLACK);
    display.setCursor(utx, uty);
    display.print(text);
    display.setCursor(umx, umy);
    display.print(text);
  } while (display.nextPage());
}

// MQTT

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  // displayPartial("MQTT");
  drawString(SCREEN_WIDTH, 0, "MQTT", RIGHT);
  mqttClient.subscribe(MQTT_TOPIC, 2);
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
  drawString(SCREEN_WIDTH / 2, 0, ret, CENTER);
  //displayPartial(ret);
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
  String ret = "HZ: ";
  ret += output;

  Serial.println(ret);

  // displayPartial(ret);
  drawString(SCREEN_WIDTH / 2, 0, ret, CENTER);
  // Disconnect
  http.end();
}

void DisplayData() {                 // 4.2" e-paper display is 400x300 resolution
  DrawHeadingSection();                 // Top line of the display
}

void DrawHeadingSection() {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  // drawString(SCREEN_WIDTH / 2, 0, "City", CENTER);
  // drawString(SCREEN_WIDTH, 0, "date_str", RIGHT);
  // drawString(4, 0, "time_str", LEFT);
  // DrawBattery(65, 12);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, GxEPD_BLACK);
}

void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}

void InitialiseDisplay() {
  display.init(115200, true, 2, false);
  display.setRotation(1);
  u8g2Fonts.begin(display); // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}
