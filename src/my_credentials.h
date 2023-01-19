// Change to your WiFi credentials
const char* wifi_ssid = "your_SSID";
const char* wifi_pass = "your_PASSWORD";

// MQTT server credentials
const char* mqtt_server = "192.168.22.5";
const char* mqtt_port = "1883";
const char mqtt_topic[] = "shellies/HZ_DG/status/switch:0";

// Fetch
const char url[] = "http://192.168.22.70/rpc/Shelly.GetStatus";
