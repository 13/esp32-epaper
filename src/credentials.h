#include <Arduino.h>

// Change to your WiFi credentials
const char* ssid     = "";
const char* password = "";

// MQTT server credentials
const char* mqtt_server = "192.168.22.5";
uint16_t mqtt_port = 1883;
const char* mqtt_topic = "shellies/HZ_DG/status/switch:0";
const char* mqtt_topics[] = { "shellies/HZ_DG/status/switch:0", 
                              "shellies/HZ_DG/status/switch:0" };

// Fetch
const char* http_url = "http://192.168.22.70/rpc/Shelly.GetStatus";
const char* http_urls[] = { "http://192.168.22.70/rpc/Shelly.GetStatus" };
