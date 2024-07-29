// credentials.cpp

#include <Arduino.h>
#include "credentials.h"

const char* mqtt_topics[] = { 
    "shellies/HZ_DG/status/switch:0", 
    "muh/sensors/22/json",
    "muh/wst/data/B327",
    NULL  // Indicates the end of the array
};

const char* http_urls[] = { 
    "http://192.168.22.70/rpc/Shelly.GetStatus",
    NULL
};

const char* ntpServers[] = { 
    "pool.ntp.org", 
    "time.nist.gov",
    NULL
};

const long gmtOffset_sec = 3600;       // GMT offset in seconds (1 hour)
const int daylightOffset_sec = 3600;   // Daylight saving time offset in seconds
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Time zone for Rome

