#include <Arduino.h>

// Change to your WiFi credentials
const char* ssid     = "";
const char* password = "";

// MQTT server credentials
const char* mqtt_server = "192.168.22.5";
uint16_t mqtt_port = 1883;
const char* mqtt_topic = "shellies/HZ_DG/status/switch:0";
const char* mqtt_topics[] = { "shellies/HZ_DG/status/switch:0", 
                              "sensors/f9",
                              "sensors/3f" };
// Fetch URLs
const char* http_urls[] = { "http://192.168.22.70/rpc/Shelly.GetStatus" };
