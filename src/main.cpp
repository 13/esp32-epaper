#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// MQTT server credentials
#define MQTT_HOST IPAddress(192, 168, 22, 5)
#define MQTT_PORT 1883
#define MQTT_TOPIC "sensors/3f/json"

AsyncMqttClient mqttClient;

// Initialize the e-paper display
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=*/5, /*DC=*/17, /*RST=*/16, /*BUSY=*/4)); // GDEW042T2 400x300, UC8176 (IL0398)

void bootScreen(String text);
void helloFullScreenPartialMode(String text);
void onMqttConnect(bool sessionPresent);
void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

void setup()
{
  Serial.begin(115200);
  delay(10);
  delay(4000);
  // boot
  Serial.printf("\n\nBooting... Compiled: %s\n", __TIMESTAMP__);
  String boot = "Booting...\nCompiled: ";
  boot += __TIMESTAMP__;

  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  bootScreen("HELLO");
  delay(1000);
  display.powerOff();
  Serial.println("setup done");

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  helloFullScreenPartialMode("WIFI");

  // Initialize the MQTT client
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.connect();
}

void loop()
{
}

// void bootScreen(const char* text)
void bootScreen(String text)
{
  // Serial.println("helloWorld");
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
  Serial.println("helloWorld done");
}

void helloFullScreenPartialMode(String text)
{
  Serial.println("helloFullScreenPartialMode");
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
    display.setCursor(utx, uty);
    display.print(text);
    display.setCursor(umx, umy);
    display.print(text);
  } while (display.nextPage());
  Serial.println("helloFullScreenPartialMode done");
}

// MQTT

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  helloFullScreenPartialMode("MQTT");
  mqttClient.subscribe(MQTT_TOPIC, 2);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  // Parse the JSON data
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, len);
  serializeJsonPretty(doc, Serial);
  Serial.println();

  float t1 = doc["T1"];
  float t2 = doc["T2"];
  float t3 = doc["T3"];

  String output = "T1: ";
  output += t1;
  output += " T2: ";
  output += t2;
  output += " T3: ";
  output += t3;

  bootScreen(output);
}

// Fetch

void onData(void* arg, AsyncClient* client, void* data, size_t len) {
  StaticJsonDocument<200> json;
  DeserializationError error = deserializeJson(json, (char*)data);
  if (error) {
    // handle error
  } else {
    // use json data
  }
  client->close();
  delete client;
}

void fetchJsonAsync(const char* url) {
  AsyncClient* client = new AsyncClient();
  client->onData(onData, NULL);
  // parse the url to get the server and path
  int port = 80;
  String server, path;
  // code to parse the url and extract server and path
  client->connect(server.c_str(), port);
  client->add("GET " + path + " HTTP/1.1\r\n");
  client->add("Host: " + server + "\r\n");
  client->add("Connection: close\r\n\r\n");
}