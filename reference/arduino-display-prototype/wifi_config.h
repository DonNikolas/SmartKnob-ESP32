#ifndef KNOB_WIFI_CONFIG_H
#define KNOB_WIFI_CONFIG_H

// Zuhause eintragen. Nicht mit echten Zugangsdaten weitergeben!
// ESP32-S3: 2,4-GHz-WLAN, normales WPA/WPA2-Personal-Netz.
#ifndef KNOB_WIFI_SSID
#define KNOB_WIFI_SSID "SSID"
#endif
#ifndef KNOB_WIFI_PASSWORD
#define KNOB_WIFI_PASSWORD "PASSWORD"
#endif

// Deutschland / Oesterreich / Schweiz: automatische Sommer-/Winterzeit.
#define KNOB_TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define KNOB_NTP_SERVER_1 "pool.ntp.org"
#define KNOB_NTP_SERVER_2 "time.cloudflare.com"
#define KNOB_WIFI_RETRY_MS 30000UL

#endif
