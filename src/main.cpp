#include <Arduino.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>        // Built-in
#include "time.h"        // Built-in
#include "esp_system.h"
// Display
#include <SPI.h> // Built-in
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "SFProTextBold32.h"
#include "SFProTextBold55.h"

#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#define SPIFFS LittleFS
#include <LittleFS.h>

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
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
long mqttLastReconnectAttempt = 0;

StaticJsonDocument<512> wsJson;

String hostname = "esp32-";

// Led Pin
const int ledPin = 2;

String wsSerializeJson(StaticJsonDocument<512> djDoc)
{
  String jsonStr;
  // wsJson["uptime"] = printUptime();
  wsJson["wifi"]["rssi"] = WiFi.RSSI();
  serializeJson(djDoc, jsonStr);
  Serial.print("> [WS] ");
  Serial.println(jsonStr);
  return jsonStr;
}

String getResetReason()
{
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason)
  {
  case ESP_RST_POWERON:
    return "Power on reset";
  case ESP_RST_EXT:
    return "External reset";
  case ESP_RST_SW:
    return "Software reset";
  case ESP_RST_PANIC:
    return "Panic (crash) reset";
  case ESP_RST_INT_WDT:
    return "Internal watchdog reset";
  case ESP_RST_TASK_WDT:
    return "Task watchdog reset";
  case ESP_RST_BROWNOUT:
    return "Brownout reset";
  case ESP_RST_SDIO:
    return "SDIO reset";
  default:
    return "Unknown reset reason";
  }
}

void getState()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("> [WiFi] IP: ");
    Serial.println(WiFi.localIP().toString());

    wsJson["wifi"]["ip"] = WiFi.localIP().toString();
    wsJson["wifi"]["mac"] = WiFi.macAddress();
    wsJson["wifi"]["ssid"] = WiFi.SSID();
    wsJson["wifi"]["rssi"] = WiFi.RSSI();
    wsJson["wifi"]["hostname"] = WiFi.getHostname();
    wsJson["wifi"]["reset"] = getResetReason();
  }
}

String getIP()
{
  return String(WiFi.localIP().toString());
}

void notifyClients()
{
  ws.textAll(wsSerializeJson(wsJson));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("> [WebSocket] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    getState();
    notifyClients();
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("> [WebSocket] Client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Replaces placeholder with LED state value
String processor(const String &var)
{
  Serial.println(var);
  if (var == "ip")
  {
    return getIP();
  }
  return String();
}

void connectToWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(wifi_ssid, wifi_pass);
  Serial.print("> [WiFi] Connecting...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" OK");
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("> [WiFi] IP: ");
    Serial.println(WiFi.localIP().toString());

    wsJson["wifi"]["ip"] = WiFi.localIP().toString();
    wsJson["wifi"]["mac"] = WiFi.macAddress();
    wsJson["wifi"]["ssid"] = WiFi.SSID();
    wsJson["wifi"]["rssi"] = WiFi.RSSI();
    wsJson["wifi"]["hostname"] = WiFi.getHostname();
    wsJson["wifi"]["reset"] = getResetReason();
  }
}

boolean connectToMqtt()
{
  drawString(SCREEN_WIDTH, 0, "MQTT", RIGHT, u8g2_font_helvB08_tf);
  String lastWillTopic = "esp/";
  lastWillTopic += hostname;
  String ipTopic = lastWillTopic;
  ipTopic += "/IP";
  lastWillTopic += "/LWT";

  if (!mqttClient.connected())
  {
    Serial.print("> [MQTT] Connecting...");
    if (mqttClient.connect(hostname.c_str(), lastWillTopic.c_str(), 1, true, "offline"))
    {
      Serial.println(" OK");
      mqttClient.publish(lastWillTopic.c_str(), "online", true);
      mqttClient.publish(ipTopic.c_str(), WiFi.localIP().toString().c_str(), true);
    }
    else
    {
      Serial.print(" failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
  else
  {
    // Serial.println("> [MQTT] Connected");
    mqttClient.publish(lastWillTopic.c_str(), "online", true);
    mqttClient.publish(ipTopic.c_str(), WiFi.localIP().toString().c_str(), true);
  }
  return mqttClient.connected();
}

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

#ifdef VERBOSE
// one minute mark
#define MARK
#define INTERVAL_1MIN (1 * 60 * 1000L)
unsigned long lastMillis = 0L;
uint32_t countMsg = 0;
#endif

// supplementary functions
#ifdef MARK
void printMARK()
{
  if (countMsg == 0)
  {
    Serial.println(F("> [MARK] Starting... OK"));
    countMsg++;
  }
  if (countMsg == UINT32_MAX)
  {
    countMsg = 1;
  }
  if (millis() - lastMillis >= INTERVAL_1MIN)
  {
    Serial.print(F("> [MARK] Uptime: "));

    if (countMsg >= 60)
    {
      int hours = countMsg / 60;
      int remMins = countMsg % 60;
      if (hours >= 24)
      {
        int days = hours / 24;
        hours = hours % 24;
        Serial.print(days);
        Serial.print(F("d "));
      }
      Serial.print(hours);
      Serial.print(F("h "));
      Serial.print(remMins);
      Serial.println(F("m"));
    }
    else
    {
      Serial.print(countMsg);
      Serial.println(F("m"));
    }
    countMsg++;
    lastMillis += INTERVAL_1MIN;

    // 1 minute status update
    connectToMqtt();
    notifyClients();
  }
}
#endif

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
  hostname += getUniqueID();
#ifdef VERBOSE
  Serial.print(("> Mode: "));
  Serial.print(F("VERBOSE "));
#ifdef DEBUG
  Serial.print(F("DEBUG"));
#endif
  Serial.println();
#endif

  // Initialize LittleFS
  if (!LittleFS.begin())
  {
    Serial.println(F("> [LittleFS] ERROR "));
    return;
  }
  connectToWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  if (WiFi.status() == WL_CONNECTED)
  {
    connectToMqtt();
  }

  pinMode(ledPin, OUTPUT);
  blinkLED();

  display.display(false); // reset screen

  if (startWiFi() == WL_CONNECTED)
  {
    if (MDNS.begin(hostname.c_str()))
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
    ArduinoOTA.setHostname(hostname.c_str());

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

  initWebSocket();
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", String(), false, processor); });
  server.on("/css/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/css/bootstrap.min.css", "text/css"); });
  server.on("/js/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/js/bootstrap.bundle.min.js", "text/javascript"); });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/favicon.ico", "image/x-icon"); });
  server.on("/ip", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getIP().c_str()); });
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "application/json", wsSerializeJson(wsJson)); });
  // Start server
  server.begin();
}

void loop()
{
  ws.cleanupClients();
  if (WiFi.status() != WL_CONNECTED)
  {
    connectToWiFi();
  }
  if (!mqttClient.connected())
  {
    Serial.println("> [MQTT] Not connected loop");
    long mqttNow = millis();
    if (mqttNow - mqttLastReconnectAttempt > 5000)
    {
      mqttLastReconnectAttempt = mqttNow;
      // Attempt to reconnect
      if (connectToMqtt())
      {
        mqttLastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    mqttClient.loop();
  }
  loopTime();
  ArduinoOTA.handle();
#ifdef MARK
  printMARK();
#endif
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
  String hn = "esp32-";
  hn += getUniqueID();
  WiFi.setHostname(hn.c_str());
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
  display.setRotation(1); // 3
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
  lastWillTopic += "/LWT";

  if (mqttClient.connect(clientId.c_str(), lastWillTopic.c_str(), 1, true, "offline"))
  {
    Serial.println(" OK");
    mqttClient.publish(lastWillTopic.c_str(), "online", true);
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
        ret += "° ";
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
        ret += "° ";
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
        ret += "° ";
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
  String stateTopic = lastWillTopic;
  stateTopic += "/json";
  lastWillTopic += "/LWT";

  StaticJsonDocument<200> doc;
  doc["hn"] = WiFi.getHostname();
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["mac"] = WiFi.macAddress();

  String jsonString;
  serializeJson(doc, jsonString);

  if (mqttClient.connect(clientId.c_str(), lastWillTopic.c_str(), 1, true, "offline"))
  {
    Serial.println(" OK");

    mqttClient.publish(lastWillTopic.c_str(), "online", true);
    mqttClient.publish(stateTopic.c_str(), jsonString.c_str());

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
