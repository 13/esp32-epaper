#include <Arduino.h>

#define VERBOSE
// #define DEBUG

/* WiFi */
const char* wifi_ssid = "network";
const char* wifi_pass  = "";

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
const char* mqtt_topic = "muh/sensors";
const char* mqtt_topic_lwt = "muh/esp";
uint16_t mqtt_port = 1883;
const char* mqtt_topics[] = { "shellies/HZ_DG/status/switch:0", 
                              "muh/sensors/22/json",
                              "muh/sensors/d5/json",
                              NULL };
                              //"muh/sensors/d5/json""muh/WStation/data/B327",
// Fetch URLs
const char* http_urls[] = { "http://192.168.22.70/rpc/Shelly.GetStatus" };

// Time
const char* ntpServers[] = { "pool.ntp.org", "time.nist.gov" };
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Rome

/*
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#define VERBOSE
// #define DEBUG
// #define DEBUG_CRC
// #define REQUIRES_INTERNET
#define MQTT_SUBSCRIBE

// #define REQUIRES_INTERNET
#define MQTT_SUBSCRIBE

/* Device */
#define DEVICE_DESCRIPTION "ePaperKommer"

/* WiFi */
#define WIFI_SSID ""
#define WIFI_PASS ""

/* MQTT server credentials */
#define MQTT_USER ""
#define MQTT_PASS ""
#define MQTT_SERVER "192.168.22.5"
#define MQTT_TOPIC "muh/sensors"
#define MQTT_TOPIC_LWT "muh/esp"
#define MQTT_PORT 1881 // 1881
#ifdef MQTT_SUBSCRIBE
#define MQTT_SUBSCRIBE_TOPIC "muh/sensors/#"
const char* mqtt_topics[] = { "shellies/HZ_DG/status/switch:0", 
                              "muh/sensors/22/json",
                              "muh/WStation/data/B327",
                              NULL };
#endif
// Fetch URLs
const char* http_urls[] = { "http://192.168.22.70/rpc/Shelly.GetStatus" };

// Time
const char* ntpServers[] = { "pool.ntp.org", "time.nist.gov" };
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Rome

#endif // CREDENTIALS_H
*/