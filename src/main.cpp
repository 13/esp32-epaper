#include <Arduino.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>        // Built-in
#include "time.h"        // Built-in
#include <SPI.h>         // Built-in
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "SFProTextBold32.h"
#include "SFProTextBold55.h"

#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "credentials.h" // Wifi SSID and PASSWORD

// #define DEBUG

#define SCREEN_WIDTH 300.0 // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 400.0

enum alignment
{
  LEFT,
  RIGHT,
  CENTER
};

#if defined(ESP32)
// Connections for e.g. LOLIN D32
/*static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
static const uint8_t EPD_CS = 5;    // to EPD CS
static const uint8_t EPD_RST = 16;  // to EPD RST
static const uint8_t EPD_DC = 17;   // to EPD DC
static const uint8_t EPD_SCK = 18;  // to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23; // to EPD DIN*/

// Wemos S2 mini
static const uint8_t EPD_BUSY = 9;  // to EPD BUSY
static const uint8_t EPD_CS = 10;   // to EPD CS
static const uint8_t EPD_RST = 7;   // to EPD RST
static const uint8_t EPD_DC = 8;    // to EPD DC
static const uint8_t EPD_SCK = 12;  // to EPD CLK
static const uint8_t EPD_MISO = 13; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 11; // to EPD DIN
#endif

#if defined(ESP8266)
// mapping suggestion from Waveshare SPI e-Paper to Wemos D1 mini
// BUSY -> D2, RST -> D4, DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V
// NOTE: connect 3.3k pull-down from D8 to GND if your board or shield has level converters
// NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, use different pin in case, or 4k7 pull-down
static const uint8_t EPD_BUSY = 4;  // connect EPD BUSY to GPIO 4 (D2)
static const uint8_t EPD_CS = 15;   // connect EPD CS to GPIO 15 (D8)
static const uint8_t EPD_RST = 2;   // connect EPD RST to GPIO 2 (D4)
static const uint8_t EPD_DC = 0;    // connect EPD DC to GPIO 0 (D3)
static const uint8_t EPD_SCK = 14;  // connect EPD CLK to GPIO 14 (D5)
static const uint8_t EPD_MISO = 12; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 13; // connect EPD DIN to GPIO 13 (D7)
#endif

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts; // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

int wifi_signal;
long StartTime = 0;
unsigned long previousMinute = 0;
long lastReconnectAttempt = 0;

// WiFi & MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
HTTPClient httpClient;

// Led Pin
const int ledPin = 2;

// MQTT
void initMqtt();
void onMqttDisconnect();
void onMqttMessage(char *topic, byte *payload, unsigned int len);
boolean reconnectMqtt(String uid);
void fetchJson(const char *url);
void initDisplay();
void drawSections();
void drawString(int x, int y, String text, alignment align, const uint8_t *font);
void drawStringLine(int x, int y, String text, alignment align, const uint8_t *font);
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align);
void displayData();
uint8_t startWiFi();
void blinkLED();
void printLocalTime();
void printLocalTime(boolean updateTime);
void loopTime();

// Last 4 digits of ChipID
String getUniqueID()
{
  String uid = "0";
  uid = WiFi.macAddress().substring(12);
  uid.replace(":", "");
  return uid;
}

void setup()
{
  StartTime = millis();
  Serial.begin(115200);
  delay(10);
#ifdef VERBOSE
  delay(5000);
#endif
  // Start Boot
  Serial.println(F("> "));
  Serial.println(F("> "));
  Serial.print(F("> Booting... Compiled: "));
  Serial.println(GIT_VERSION);
  Serial.print(F("> Node ID: "));
  Serial.println(getUniqueID());
#ifdef VERBOSE
  Serial.print(("> Mode: "));
  Serial.print(F("VERBOSE "));
#ifdef DEBUG
  Serial.print(F("DEBUG"));
#endif
  Serial.println();
#endif

  pinMode(ledPin, OUTPUT);
  blinkLED();

  display.display(false); // reset screen

  if (startWiFi() == WL_CONNECTED)
  {
    if (MDNS.begin(hostname))
    {
      Serial.println("[MDNS]: Responder started");
    }
    initDisplay(); // Give screen time to initialise by getting weather data!
    // WiFiClient client; // wifi client object
    drawString(4, 0, "WiFi", LEFT, u8g2_font_helvB08_tf);

    // Time
    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    configTzTime(time_zone, ntpServers[0], ntpServers[1]);
    printLocalTime(true);

    // FetchJson
    for (int i = 0; i < sizeof(http_urls) / sizeof(http_urls[0]); i++)
    {
      Serial.print("[FETCH]: Fetching ");
      Serial.print(http_urls[i]);
      Serial.println(" ...");
      fetchJson(http_urls[i]);
    }

    // Initialize the MQTT client
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(onMqttMessage);
    // mqttClient.setCallback(onMqttMessage);

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
      Serial.println("[OTA]: Start updating " + type); })
        .onEnd([]()
               { Serial.println("\n[OTA]: End"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("[OTA]: Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error)
                 {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("[OTA]: Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("[OTA]: Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("[OTA]: Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("[OTA]: Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("[OTA]: End Failed"); });

    ArduinoOTA.begin();

    displayData();
  }
  else
  {
    drawString(4, 0, "XXXX", LEFT, u8g2_font_helvB08_tf);
  }
}

void loop()
{
  // this will never run!
  if (!mqttClient.connected())
  {
    long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnectMqtt(getUniqueID()))
      {
        lastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    // Client connected
    mqttClient.loop();
  }
  loopTime();
  ArduinoOTA.handle();
}

void blinkLED()
{
  if (enableLED)
  {
    for (int i = 0; i < 3; i++)
    {
      digitalWrite(ledPin, HIGH);
      delay(50);
      digitalWrite(ledPin, LOW);
      delay(50);
    }
  }
}

void drawSections()
{
  // u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  // display.drawLine(0, 12, SCREEN_WIDTH, 12, GxEPD_BLACK);

  // display.drawLine(0, (SCREEN_HEIGHT / 1.8) + 35, SCREEN_WIDTH, (SCREEN_HEIGHT / 1.8) + 35, GxEPD_BLACK);
  // drawString(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 1.8) + 55, "INSIDE", CENTER);

  // display.drawLine(0, (SCREEN_HEIGHT / 2.8) + 50, SCREEN_WIDTH, (SCREEN_HEIGHT / 2.8) + 50, GxEPD_BLACK);
  // drawStringLine(SCREEN_WIDTH / 2.8, (SCREEN_HEIGHT / 2.8) + 40, "OUTSIDE", CENTER);
}

uint8_t startWiFi()
{
  Serial.print("\r\n[WiFi] Connecting to: ");
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
    Serial.println("[WiFi]: Connected at: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("[WiFi]: Connection *** FAILED ***");
    drawString(4, 0, "XXXX", LEFT, u8g2_font_helvB08_tf);
  }
  return connectionStatus;
}

void drawString(int x, int y, String text, alignment align, const uint8_t *font)
{
  u8g2Fonts.setFont(font);
  char textArray[text.length() + 1];
  text.toCharArray(textArray, text.length() + 1);
  int16_t textWidth = u8g2Fonts.getUTF8Width(textArray);
  int16_t textAscent = u8g2Fonts.getFontAscent();
  int16_t textDescent = u8g2Fonts.getFontDescent();
  int16_t boxWidth = textWidth + 2;
  int16_t boxHeight = textAscent + textDescent + 6; // 10

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
  display.fillRect(x - 4, y + h - boxHeight + 4, boxWidth + 8, boxHeight, GxEPD_WHITE);
  // display.drawRect(x - 4, y + h - boxHeight + 4, boxWidth + 8, boxHeight, GxEPD_BLACK);
  u8g2Fonts.setCursor(x, y + h + 2); // +2
  u8g2Fonts.print(text);
  display.display(true); // partial update
}

void drawStringLine(int x, int y, String text, alignment align, const uint8_t *font)
{
  u8g2Fonts.setFont(font);
  char textArray[text.length() + 1];
  text.toCharArray(textArray, text.length() + 1);
  int16_t textWidth = u8g2Fonts.getUTF8Width(textArray);
  int16_t textAscent = u8g2Fonts.getFontAscent();
  int16_t textDescent = u8g2Fonts.getFontDescent();
  int16_t boxWidth = textWidth + 2;
  int16_t boxHeight = textAscent - textDescent + 4; // + textDescent + 15; // 10
#ifdef DEBUG
  Serial.printf("[DEBUG]: drawStringLine box:\ntxtW: %d, txtAsc: %d, txtDesc: %d, boxW: %d, boxH: %d\n", textWidth, textAscent, textDescent, boxWidth, boxHeight);
#endif
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
#ifdef DEBUG
  Serial.printf("[DEBUG]: drawStringLine text:\nx: %d, y: %d, x1: %d, y1: %d, w: %d, h: %d\n", x, y, &x1, &y1, &w, &h);
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
  u8g2Fonts.setCursor(x, y + h + 2); // +2
  u8g2Fonts.print(text);
  display.display(true); // partial update
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
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
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
  // u8g2Fonts.setFont(u8g2_font_helvB08_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

void displayData()
{
  drawSections(); // Top line of the display
}

// Fetch
void fetchJson(const char *url)
{
  httpClient.useHTTP10(true);
  httpClient.begin(wifiClient, url);
  httpClient.GET();

  // Parse response
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, httpClient.getStream());

  String ret;

  if (doc.containsKey("switch:0"))
  {
    bool output = doc["switch:0"]["output"];
    ret = "HZG: ";
    ret += output ? "Ein" : "Aus";
    if (!ret.isEmpty())
    {
      drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.03, ret, CENTER, SFProTextBold32);
    }
  }
  if (!ret.isEmpty())
  {
    Serial.print("[FETCH]: Fetching... ");
    Serial.println(ret);
    printLocalTime();
    blinkLED();
  }

  // Disconnect
  httpClient.end();
}

void printLocalTime()
{
  printLocalTime(false);
}

void printLocalTime(boolean updateTime)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("[TIME]: Failed to obtain time");
    return;
  }
  Serial.print("[TIME]: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  char timeBuff[6];
  strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
  String ret = timeBuff;

  if (updateTime)
  {
    drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 5.0, ret, CENTER, SFProTextBold55);
  }
  else
  {
    drawString(SCREEN_WIDTH / 2, 0, ret, CENTER, u8g2_font_helvB08_tf);
  }
}

void loopTime()
{
  time_t currentTime;
  time(&currentTime);
  struct tm *timeinfo = localtime(&currentTime);

  if (timeinfo->tm_min != previousMinute)
  {
    previousMinute = timeinfo->tm_min;
    Serial.print("[TIME]: ");
    Serial.println(ctime(&currentTime));
    printLocalTime(true);
  }
}

// MQTT
void initMqtt()
{
  // Connect to MQTT broker
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(onMqttMessage);
}
void onMqttConnect(bool sessionPresent, String uid)
{
  /*Serial.println("Connected to MQTT broker");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  mqttClient.subscribe("test/topic");*/

  drawString(SCREEN_WIDTH, 0, "MQTT", RIGHT, u8g2_font_helvB08_tf);
  Serial.print("> [MQTT] Connecting...");

  String clientId = "esp32-";
  clientId += uid;
  String lastWillTopic = "esp/";
  lastWillTopic += clientId;
  lastWillTopic += "/status";

  if (mqttClient.connect(clientId.c_str(), lastWillTopic.c_str(), 1, true, "offline"))
  {
    Serial.println(" OK");
    mqttClient.publish(lastWillTopic.c_str(), "online");
    Serial.print("> [MQTT] Subscribing... ");
    for (int i = 0; i < sizeof(mqtt_topics) / sizeof(mqtt_topics[0]); i++)
    {
      Serial.print(mqtt_topics[i]);
      if (i == sizeof(mqtt_topics) / sizeof(mqtt_topics[0]))
      {
        Serial.print(" ");
      }
      else
      {
        Serial.print(", ");
      }
      mqttClient.subscribe(mqtt_topics[i]);
    }
  }
  else
  {
    Serial.print(" failed, rc=");
    Serial.println(mqttClient.state());
  }
}

void onMqttDisconnect()
{
  drawString(SCREEN_WIDTH, 0, "XXXX", RIGHT, u8g2_font_helvB08_tf);
  Serial.print("[MQTT]: Disconnected... ");
  for (int i = 0; i < sizeof(mqtt_topics) / sizeof(mqtt_topics[0]); i++)
  {
    Serial.print(mqtt_topics[i]);
    Serial.print(", ");
    // mqttClient.subscribe(mqtt_topics[i], 2);
  }
  Serial.println("");
  /*if (WiFi.isConnected())
  {
    Serial.println("[MQTT]: Reconnecting...");
    reconnectMqtt();
    // check if disconnected
    // ESP.restart();
  }*/
}

void onMqttMessage(char *topic, byte *payload, unsigned int len)
{
  /*Serial.print("Received message on topic: ");
  Serial.print(topic);
  Serial.print(", payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();*/

  // Parse the JSON data
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, len);
  serializeJsonPretty(doc, Serial);
  Serial.println();

  String ret;

  if (doc.containsKey("N"))
  {
    if (doc["N"] == "22")
    {
      if (doc.containsKey("T2") && doc.containsKey("T4"))
      {
        float t2_22 = doc["T2"];
        float t4_22 = doc["T4"];
        ret += String((t2_22 + t4_22) / 2, 1);
        ret += "?? ";
      }
      if (doc.containsKey("H4"))
      {
        float h1_22 = doc["H4"];
        ret += String(h1_22, 1);
        ret += "%";
      }
      if (!ret.isEmpty())
      {
        drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.31, ret, CENTER, SFProTextBold32);
      }
    }
    if (doc["N"] == "87")
    {
      if (doc.containsKey("T1") && doc.containsKey("T2"))
      {
        float t1_87 = doc["T1"];
        float t2_87 = doc["T2"];
        ret += String((t1_87 + t2_87) / 2, 1);
        ret += "?? ";
      }
      if (doc.containsKey("H1"))
      {
        float h1_87 = doc["H1"];
        ret += String(h1_87, 1);
        ret += "%";
      }
      if (!ret.isEmpty())
      {
        drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.31, ret, CENTER, SFProTextBold32);
      }
    }
    if (doc["N"] == "d5")
    {
      if (doc.containsKey("T1") && doc.containsKey("T2"))
      {
        float t1_d5 = doc["T1"];
        float t2_d5 = doc["T2"];
        ret += String((t1_d5 + t2_d5) / 2, 1);
        ret += "?? ";
      }
      if (doc.containsKey("H1"))
      {
        float h1_d5 = doc["H1"];
        ret += String(h1_d5, 1);
        ret += "%";
      }
      if (!ret.isEmpty())
      {
        drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2.0, ret, CENTER, SFProTextBold32);
      }
    }
  }

  if (doc.containsKey("output"))
  {
    bool output = doc["output"];
    ret += "HZG: ";
    ret += output ? "Ein" : "Aus";
    if (!ret.isEmpty())
    {
      drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.03, ret, CENTER, SFProTextBold32);
    }
  }

  if (!ret.isEmpty())
  {
    Serial.print("[MQTT]: ");
    Serial.println(ret);
    printLocalTime();
    blinkLED();
  }
}

boolean reconnectMqtt(String uid)
{
  drawString(SCREEN_WIDTH, 0, "MQTT", RIGHT, u8g2_font_helvB08_tf);
  Serial.print("> [MQTT] Connecting...");

  String clientId = "esp32-";
  clientId += uid;
  String lastWillTopic = "esp/";
  lastWillTopic += clientId;
  lastWillTopic += "/status";

  if (mqttClient.connect(clientId.c_str(), lastWillTopic.c_str(), 1, true, "offline"))
  {
    Serial.println(" OK");
    mqttClient.publish(lastWillTopic.c_str(), "online");
    Serial.print("> [MQTT] Subscribing... ");
    for (int i = 0; i < sizeof(mqtt_topics) / sizeof(mqtt_topics[0]); i++)
    {
      Serial.print(mqtt_topics[i]);
      if (i == sizeof(mqtt_topics) / sizeof(mqtt_topics[0]))
      {
        Serial.print(" ");
      }
      else
      {
        Serial.print(", ");
      }
      mqttClient.subscribe(mqtt_topics[i]);
    }
  }
  else
  {
    Serial.print(" failed, rc=");
    Serial.println(mqttClient.state());
  }

  return mqttClient.connected();
}
