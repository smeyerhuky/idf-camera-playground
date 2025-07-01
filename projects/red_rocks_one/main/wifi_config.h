#pragma once

// Red Rocks Photo Booth WiFi Configuration
#define WIFI_SSID      "RedRocks_PhotoBooth"
#define WIFI_PASSWORD  "Concert2024"
#define WIFI_MAX_RETRY 5

// Time synchronization (optional for photo booth)
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC -25200  // Mountain Time (UTC-7)
#define DAYLIGHT_OFFSET_SEC 3600  // Daylight saving time