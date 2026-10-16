// Generated with ChatGPT assistance. adsb.fi personal/non-commercial API.
// Separate core-0 task; no motor writes, no NVS writes, no insecure TLS fallback.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <time.h>
#include "../network_setup/config.h"
#include "radar_parse.h"
#include "radar_roots.h"
namespace KnobRadar {
namespace {
QueueHandle_t queue = nullptr;
std::atomic<bool> enabled{false};
Snapshot snapshot;
KnobNetwork::Config config;
bool configured = false;
void publish(const char* status) {
    snprintf(snapshot.status,sizeof(snapshot.status),"%s",status);
    ++snapshot.sequence; if(queue)xQueueOverwrite(queue,&snapshot);
}
// HTTPClient decodes chunking into this bounded sink; oversized bodies fail.
class Body : public Stream {
public:
    static constexpr size_t LIMIT=131072;
    char* data=nullptr; size_t size=0;
    Body(){data=static_cast<char*>(ps_malloc(LIMIT));}
    ~Body(){free(data);}
    size_t write(uint8_t b) override {return write(&b,1);}
    size_t write(const uint8_t* p,size_t n) override {
        if(!data || n>LIMIT-size)return 0;
        memcpy(data+size,p,n);size+=n;return n;
    }
    int available() override{return 0;} int read() override{return -1;}
    int peek() override{return -1;} void flush() override{}
};
bool fetch() {
    WiFiClientSecure tls; tls.setCACert(ROOTS); tls.setTimeout(6000); tls.setHandshakeTimeout(6);
    HTTPClient http;
    char url[160];
    // API distance is nautical miles. Fetch 14 NM once, locally filter to 25 km.
    snprintf(url,sizeof(url),"https://opendata.adsb.fi/api/v3/lat/%.5f/lon/%.5f/dist/14",config.latitude,config.longitude);
    http.setConnectTimeout(6000); http.setTimeout(6000);
    http.setUserAgent("SmartKnob-ESP32/plane-radar-v1");
    if(!http.begin(tls,url)){publish("HTTPS setup failed");return false;}
    int code=http.GET();
    if(code!=200){ char text[48];snprintf(text,sizeof(text),"Data unavailable (%d)",code);publish(text);http.end();return false; }
    Body body;
    if(!body.data || http.getSize()>int(Body::LIMIT)){publish("Response too large / memory");http.end();return false;}
    int written=http.writeToStream(&body);http.end();
    // Parse into a separate snapshot: errors must not overwrite the last good data.
    Snapshot next;
    if(written<0 || !parse(body.data,body.size,config.latitude,config.longitude,next)){publish("Invalid flight response");return false;}
    if(!isfinite(next.sourceSeconds) || fabs(double(time(nullptr))-next.sourceSeconds)>60){publish("Provider data stale");return false;}
    next.valid=true;next.receivedAt=millis();next.sequence=snapshot.sequence;
    snapshot=next;publish("Live");return true;
}
void task(void*) {
    if(!configured){publish("Run wifi_setup first");vTaskDelete(nullptr);return;}
    configTime(0,0,"pool.ntp.org","time.google.com");
    uint32_t lastConnect=millis(), lastFetch=0;bool attempted=false;
    for(;;){
        uint32_t now=millis();
        if(WiFi.status()!=WL_CONNECTED){
            if(now-lastConnect>=30000){WiFi.reconnect();lastConnect=now;}
            if(strcmp(snapshot.status,"Wi-Fi disconnected"))publish("Wi-Fi disconnected");
        }else if(time(nullptr)<1700000000){
            if(strcmp(snapshot.status,"Synchronising clock"))publish("Synchronising clock");
        }else if(enabled.load() && (!attempted || now-lastFetch>=15000)){
            // Failed requests back off for 60 seconds. No requests outside Radar.
            bool ok=fetch();attempted=true;
            lastFetch=millis();
            // Use a separate delay below, rather than comparing a future uint32_t.
            if(!ok)for(unsigned i=0;i<90;++i)vTaskDelay(pdMS_TO_TICKS(500));
            lastFetch=millis();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
}
void begin(){
    queue=xQueueCreate(1,sizeof(Snapshot)); if(!queue)return;
    Preferences prefs;
    if(prefs.begin(KnobNetwork::NAMESPACE,true)){
        configured=prefs.getBytesLength(KnobNetwork::KEY)==sizeof(config) &&
            prefs.getBytes(KnobNetwork::KEY,&config,sizeof(config))==sizeof(config) && KnobNetwork::valid(config);
        prefs.end();
    }
    // Called by GUI startup before the motor is initialized/enabled.
    if(configured){WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);WiFi.setAutoReconnect(false);WiFi.begin(config.ssid,config.password);}
    if(xTaskCreatePinnedToCore(task,"plane_data",12288,nullptr,1,nullptr,0)!=pdPASS)publish("Network task failed");
}
void active(bool value){enabled.store(value);}
void read(Snapshot& out){if(queue)xQueueReceive(queue,&out,0);}
}
