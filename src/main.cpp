#include <Arduino.h>
#include "credentials.h" // Wifi SSID and PASSWORD
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>        // Built-in
#include "time.h"        // Built-in
#include <SPI.h>         // Built-in
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "SFProTextBold32.h"
#include "SFProTextBold55.h"

#include <AsyncMqttClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

/* TODO: add time */

// #define DEBUG

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

int wifi_signal;
long StartTime = 0;

// MQTT
AsyncMqttClient mqttClient;

// HTTPClient
WiFiClient client; // or WiFiClientSecure for HTTPS
HTTPClient http;

// Led Pin
const int ledPin = 2;

// MQTT
void initMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void fetchJson(const char *url);

void initDisplay();
void drawSections();
void drawString(int x, int y, String text, alignment align);
void drawStringLine(int x, int y, String text, alignment align);
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align);
void displayData();
uint8_t startWiFi();
void blinkLED();
void printLocalTime();

void setup()
{
  StartTime = millis();
  Serial.begin(115200);
  delay(10);
#ifdef VERBOSE
  delay(20);
#endif
  // Start Boot
  Serial.println(F("> "));
  Serial.println(F("> "));
  Serial.print(F("> Booting... Compiled: "));
  Serial.println(F(__TIMESTAMP__));

  pinMode(ledPin, OUTPUT);
  blinkLED();

  display.display(false); // reset screen

  if (startWiFi() == WL_CONNECTED)
  {
    if (MDNS.begin(hostname))
    {
      Serial.println("MDNS responder started");
    }
    initDisplay(); // Give screen time to initialise by getting weather data!
    // WiFiClient client; // wifi client object
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(4, 0, "WIFI", LEFT);

    // Time
    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    configTzTime(time_zone, ntpServers[0], ntpServers[1]);
    printLocalTime();

    // FetchJson
    for (int i = 0; i < sizeof(http_urls) / sizeof(http_urls[0]); i++)
    {
      Serial.print("Fetching ");
      Serial.print(http_urls[i]);
      Serial.println(" ...");
      fetchJson(http_urls[i]);
    }

    // Initialize the MQTT client
    initMqtt();

    // ArduinoOTA
    // Port defaults to 3232
    // ArduinoOTA.setPort(3232);

    // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setHostname(hostname);

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
    ArduinoOTA
        .onStart([]()
                 {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
        .onEnd([]()
               { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error)
                 {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

    ArduinoOTA.begin();

    displayData();
  }
}

void loop()
{
  // this will never run!
  ArduinoOTA.handle();
}

void blinkLED()
{
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(ledPin, HIGH);
    delay(50);
    digitalWrite(ledPin, LOW);
    delay(50);
  }
}

void drawSections()
{
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  /*display.drawLine(0, 12, SCREEN_WIDTH, 12, GxEPD_BLACK);
   display.drawLine(0, (SCREEN_HEIGHT / 2.0) + 10, SCREEN_WIDTH, (SCREEN_HEIGHT / 2.0) + 10, GxEPD_BLACK);
   display.drawLine(0, (SCREEN_HEIGHT / 4.0) + 10, SCREEN_WIDTH, (SCREEN_HEIGHT / 4.0) + 10, GxEPD_BLACK);
   display.drawLine(0, (SCREEN_HEIGHT / 1.31) + 10, SCREEN_WIDTH, (SCREEN_HEIGHT / 1.31) + 10, GxEPD_BLACK);
   */
  // display.drawLine(0, (SCREEN_HEIGHT / 1.03) + 10, SCREEN_WIDTH, (SCREEN_HEIGHT / 1.03) + 10, GxEPD_BLACK);
}

uint8_t startWiFi()
{
  Serial.print("\r\nConnecting to: ");
  Serial.println(String(wifi_ssid));
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifi_ssid, wifi_pass);
  WiFi.config(ip, gw, sn, dns1);
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
  // u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  char textArray[text.length() + 1];
  text.toCharArray(textArray, text.length() + 1);
  int16_t textWidth = u8g2Fonts.getUTF8Width(textArray);
  int16_t textAscent = u8g2Fonts.getFontAscent();
  int16_t textDescent = u8g2Fonts.getFontDescent();
  int16_t boxWidth = textWidth + 2;
  int16_t boxHeight = textAscent + textDescent + 15; // 10

  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)
    x = x - w - 10;
  if (align == CENTER)
  {
    // x = x - w / 2;
    x = x - textWidth / 2;
  }
  display.fillRect(x, y - h * 2, boxWidth, boxHeight, GxEPD_WHITE);
  display.display(true);
  u8g2Fonts.setCursor(x, y + h + 2); // +2

  u8g2Fonts.print(text);
}

void drawStringLine(int x, int y, String text, alignment align)
{
  // u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  char textArray[text.length() + 1];
  text.toCharArray(textArray, text.length() + 1);
  int16_t textWidth = u8g2Fonts.getUTF8Width(textArray);
  int16_t textAscent = u8g2Fonts.getFontAscent();
  int16_t textDescent = u8g2Fonts.getFontDescent();
  int16_t boxWidth = textWidth + 2;
  int16_t boxHeight = textAscent - textDescent + 4; // + textDescent + 15; // 10
#ifdef DEBUG
  Serial.printf("drawStringLine box:\ntxtW: %d, txtAsc: %d, txtDesc: %d, boxW: %d, boxH: %d\n", textWidth, textAscent, textDescent, boxWidth, boxHeight);
#endif
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
#ifdef DEBUG
  Serial.printf("drawStringLine text:\nx: %d, y: %d, x1: %d, y1: %d, w: %d, h: %d\n", x, y, &x1, &y1, &w, &h);
#endif
  if (align == RIGHT)
    x = x - w - 10;
  if (align == CENTER)
  {
    // x = x - w / 2;
    x = x - textWidth / 2;
  }

#ifdef DEBUG
  display.drawRect(0, y + h - boxHeight + 8, SCREEN_WIDTH, boxHeight, GxEPD_BLACK);
  // display.drawRect(0, y, SCREEN_WIDTH, boxHeight, GxEPD_BLACK); // y - h * 4
#else
  display.fillRect(0, y + h - boxHeight + 8, SCREEN_WIDTH, boxHeight, GxEPD_WHITE);
  display.drawLine(0, y + h - boxHeight + 8, SCREEN_WIDTH, y + h - boxHeight + 8, GxEPD_BLACK);
  // display.fillRect(0, y - h * 2, SCREEN_WIDTH, boxHeight, GxEPD_WHITE);
#endif
  display.display(true);
  u8g2Fonts.setCursor(x, y + h + 2); // +2

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

void initDisplay()
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

void displayData()
{
  drawSections();        // Top line of the display
  display.display(true); // Full screen update mode
}

// MQTT
void initMqtt()
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
  display.display(true);
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

  String ret;

  if (doc.containsKey("N"))
  {
    u8g2Fonts.setFont(SFProTextBold32);
    if (doc["N"] == "22")
    {
      if (doc.containsKey("T2") && doc.containsKey("T4"))
      {
        float t2 = doc["T2"];
        float t4 = doc["T4"];
        ret += String((t2 + t4) / 2, 1);
        ret += "° ";
      }
      if (doc.containsKey("H4"))
      {
        float h1 = doc["H4"];
        ret += String(h1, 1);
        ret += "%";
      }
      drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.31, ret, CENTER);
    }
    if (doc["N"] == "3f")
    {
      if (doc.containsKey("T1") && doc.containsKey("T2"))
      {
        float t1 = doc["T1"];
        float t2 = doc["T2"];
        ret += String((t1 + t2) / 2, 1);
        ret += "° ";
      }
      if (doc.containsKey("H1"))
      {
        float h1 = doc["H1"];
        ret += String(h1, 1);
        ret += "%";
      }
      drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2.0, ret, CENTER);
    }
  }

  if (doc.containsKey("output"))
  {
    bool output = doc["output"];
    ret += "HZG: ";
    ret += output ? "Ein" : "Aus";
    // u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    // drawString(SCREEN_WIDTH / 2, 0, ret, CENTER);
    u8g2Fonts.setFont(SFProTextBold32);
    drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.03, ret, CENTER);
  }
  if (!ret.isEmpty())
  {
    Serial.println(ret);
    printLocalTime();
    display.display(true);
    blinkLED();
  }
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

  String ret;

  if (doc.containsKey("switch:0"))
  {
    bool output = doc["switch:0"]["output"];
    ret = "HZG: ";
    ret += output ? "Ein" : "Aus";
    // drawString(SCREEN_WIDTH / 2, 0, ret, CENTER);
    // u8g2Fonts.setFont(u8g2_font_logisoso42_tf);
    u8g2Fonts.setFont(SFProTextBold32);
    drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.03, ret, CENTER);
  }
  if (!ret.isEmpty())
  {
    Serial.println(ret);
    display.display(true);
    blinkLED();
  }

  // Disconnect
  http.end();
}

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  char timeBuff[6];
  strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
  String ret = timeBuff;

  // u8g2Fonts.setFont(u8g2_font_logisoso42_tf);
  u8g2Fonts.setFont(SFProTextBold55);
  drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 5.0, ret, CENTER);
  // drawStringLine(SCREEN_WIDTH / 2, 12, ret, CENTER);
}