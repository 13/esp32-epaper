#include <Arduino.h>

// Change to your WiFi credentials
const char* ssid     = "";
const char* password = "";

// Hostname
//const char* hostname = "epaper";
const char* ota_pass = "epr101";

// Static IP
IPAddress ip(192, 168, 22, 90);
IPAddress sn(255, 255, 255, 0);
IPAddress gw(192, 168, 22, 6);
IPAddress dns1(192, 168, 22, 6);
IPAddress dns2(8, 8, 8, 8);

// MQTT server credentials
const char* mqtt_user = "";
const char* mqtt_pass = "";
const char* mqtt_server = "192.168.22.5";
uint16_t mqtt_port = 1883;
const char* mqtt_topics[] = { "shellies/HZ_DG/status/switch:0", 
                              "sensors/22/json",
                              "sensors/3f/json" };
// Fetch URLs
const char* http_urls[] = { "http://192.168.22.70/rpc/Shelly.GetStatus" };

// Time
const char* ntpServers[] = { "pool.ntp.org", "time.nist.gov" };
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Rome

// LED
const boolean enableLED = false;