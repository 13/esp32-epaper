#include <Arduino.h>
#include "../include/version.h"
#include <wsData.h>
#include <helpers.h>
#include "epaper.h"
#include "credentials.h"

// ePaper
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// ESP
#if defined(ESP8266)
String hostname = "esp8266-";
#endif
#if defined(ESP32)
String hostname = "esp32-";
#endif
// WiFi & MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
wsData myData;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);

long mqttLastReconnectAttempt = 0;
int wsDataSize = 0;
uint8_t connectedClients = 0;

unsigned long previousMinute = 0;


// supplementary functions
#ifdef VERBOSE
// one minute mark
#define MARK
unsigned long lastMillisMark = 0L;
uint32_t countMsg = 0;
#endif

void initSerial()
{
  Serial.begin(115200);
  delay(10);
}

void printBootMsg()
{
#ifdef DEBUG
  delay(5000);
#endif
  // Start Boot
  delay(1000);
  Serial.println(F("> "));
  Serial.println(F("> "));
  Serial.print(F("> Booting... Compiled: "));
  Serial.println(VERSION);
#if defined(ESP8266) || defined(ESP32)
  Serial.print(F("> Node ID: "));
  Serial.println(getUniqueID());
  hostname += getUniqueID();
#endif
#ifdef VERBOSE
  Serial.print(("> Mode: "));
  Serial.print(F("VERBOSE "));
#ifdef USE_CRYPTO
  Serial.print(F("CRYPTO "));
#endif
#ifdef DEBUG
  Serial.print(F("DEBUG "));
#endif
#ifdef GD0
  Serial.print(F(" GD0"));
  Serial.print(GD0);
#endif
  Serial.println();
#endif
}

void setup()
{
  initSerial();
  printBootMsg();
  initFS();
  checkWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  display.display(false); // ePaper reset screen
  if (WiFi.status() == WL_CONNECTED)
  {
    initMDNS();
    connectToMqtt();
#ifdef MQTT_SUBSCRIBE
    mqttClient.setCallback(onMqttMessage);
#endif
    timeClient.begin();
    timeClient.update();
    myData.boottime = timeClient.getEpochTime();

    // ePaper
    initDisplay(); // Give screen time to initialise by getting weather data!
    drawString(4, 0, "WiFi", LEFT, u8g2_font_helvB08_tf);

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
    displayData();
  }
  else
  {
    // wifi not connected
    drawString(4, 0, "XXXX", LEFT, u8g2_font_helvB08_tf);
  }
  // Initalize websocket
  initWebSocket();
}

void loop()
{
  ws.cleanupClients();
  checkMqtt();
#ifdef MARK
  printMARK();
#endif
}
