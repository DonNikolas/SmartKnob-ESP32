// Separate core-0 worker. No motor access, NVS writes or blocking GUI requests.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <time.h>
#include <string.h>
#include "home_feeds_parse.h"
#include "home_feeds_roots.h"
namespace KnobHomeFeeds {
namespace {
QueueHandle_t queue=nullptr;
std::atomic<int> selected{-1};
Snapshot snapshot;
uint32_t attempts[COUNT]={};bool attempted[COUNT]={};
void status(int index,const char* text){
    snprintf(snapshot.readings[index].status,sizeof(snapshot.readings[index].status),"%s",text);
    ++snapshot.sequence;xQueueOverwrite(queue,&snapshot);
}
class Body : public Stream {
public:
    char data[257]={};size_t size=0;
    size_t write(uint8_t c) override{return write(&c,1);}
    size_t write(const uint8_t* p,size_t n) override{
        if(n>256-size)return 0;memcpy(data+size,p,n);size+=n;return n;
    }
    int available() override{return 0;}int read() override{return -1;}
    int peek() override{return -1;}void flush() override{}
};
void fetch(int index){
    WiFiClientSecure tls;tls.setCACert(HOME_FEED_ROOTS);tls.setTimeout(3000);tls.setHandshakeTimeout(4);
    HTTPClient http;http.setConnectTimeout(3000);http.setTimeout(3000);
    http.setUserAgent("SmartKnob-ESP32/home-sensors-v1");
    char url[100];snprintf(url,sizeof(url),"https://emon.boxtec.ch/feed/timevalue.json?id=%u",FEEDS[index].id);
    status(index,"Updating");
    if(!http.begin(tls,url)){status(index,"HTTPS setup failed");return;}
    int code=http.GET();
    if(code!=200){char text[40];snprintf(text,sizeof(text),"Fetch failed (%d)",code);http.end();status(index,text);return;}
    if(http.getSize()>256){http.end();status(index,"Response too large");return;}
    Body body;int written=http.writeToStream(&body);http.end();
    Reading next;
    if(written<0||!parse(body.data,body.size,next)){status(index,"Invalid response");return;}
    if(double(next.sourceTime)>double(time(nullptr))+60){status(index,"Invalid source time");return;}
    next.receivedAt=millis();snapshot.readings[index]=next;status(index,"OK");
}
void task(void*){
    uint32_t lastFetch=0;bool fetched=false;
    for(;;){
        int index=selected.load();uint32_t now=millis();
        if(index>=0 && index<COUNT){
            if(WiFi.status()!=WL_CONNECTED){
                if(strcmp(snapshot.readings[index].status,"Wi-Fi offline"))status(index,"Wi-Fi offline");
            }else if(time(nullptr)<1700000000){
                if(strcmp(snapshot.readings[index].status,"Waiting for clock"))status(index,"Waiting for clock");
            }else if((!fetched||now-lastFetch>=2000) && (!attempted[index]||now-attempts[index]>=15000)){
                attempted[index]=true;attempts[index]=now;fetched=true;lastFetch=now;fetch(index);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
}
void begin(){
    queue=xQueueCreate(1,sizeof(Snapshot));if(!queue)return;
    xQueueOverwrite(queue,&snapshot);
    if(xTaskCreatePinnedToCore(task,"home_feeds",12288,nullptr,1,nullptr,0)!=pdPASS){
        for(int i=0;i<COUNT;++i)status(i,"Worker unavailable");
    }
}
void select(bool active,int index){selected.store(active && index>=0 && index<COUNT?index:-1);}
void read(Snapshot& result){if(queue)xQueuePeek(queue,&result,0);}
}
