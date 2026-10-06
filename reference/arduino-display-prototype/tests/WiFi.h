#pragma once
#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
class HostWiFi {
public:
    void persistent(bool) {}
    bool mode(int) { return true; }
    bool setAutoReconnect(bool) { return true; }
    void begin(const char *, const char *);
    int status();
    bool reconnect();
};
extern HostWiFi WiFi;
