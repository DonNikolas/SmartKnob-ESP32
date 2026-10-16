// HTTP and Wi-Fi run on core 0; this module never drives the motor or writes NVS.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <stdlib.h>
#include "light_link.h"
#include "ir_link_private/config.h"
namespace KnobLight {
namespace {
WebServer server(80);
QueueHandle_t commands=nullptr,snapshots=nullptr;
struct Request {Action action;uint32_t at;};
Protocol protocol;
std::atomic<bool> available{false};
std::atomic<uint32_t> rejected{0};
uint32_t session=0;
bool number(const String& text,uint32_t& out){
    if(!text.length()||text.length()>10)return false;
    uint64_t n=0;
    for(unsigned i=0;i<text.length();++i){if(text[i]<'0'||text[i]>'9')return false;n=n*10+text[i]-'0';if(n>UINT32_MAX)return false;}
    out=(uint32_t)n;return true;
}
void poll(){
    if(server.header("X-Knob-Key")!=IR_LINK_KEY){server.send(403,"text/plain","Denied");return;}
    uint32_t boot=0,ack=0,clientSession=0;
    if(!number(server.arg("boot"),boot)||!boot||!number(server.arg("ack"),ack)||!number(server.arg("session"),clientSession)){
        server.send(400,"text/plain","Invalid request");return;
    }
    Item item=protocol.poll(boot,clientSession==session?ack:0,millis());
    // A session handshake never delivers a command from a different server boot.
    if(clientSession!=session)item=Item{};
    char body[120];snprintf(body,sizeof(body),"{\"session\":%lu,\"id\":%lu,\"cmd\":\"%s\"}",
        (unsigned long)session,(unsigned long)item.id,name(item.command));
    server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",body);
}
void task(void*){
    for(;;){
        uint32_t now=millis();protocol.expire(now);
        Request r;
        while(xQueueReceive(commands,&r,0)==pdTRUE){
            if(now-r.at>300){++protocol.dropped;continue;}
            protocol.request(r.action,now);
        }
        server.handleClient();
        Snapshot s;s.ready=true;s.online=protocol.online(millis());s.powerKnown=protocol.powerKnown;
        s.requestedOn=protocol.requestedOn;s.sent=protocol.sent;s.dropped=protocol.dropped+rejected.load();
        s.pending=protocol.count;s.lastSent=protocol.lastSent;xQueueOverwrite(snapshots,&s);
        available.store(s.online);vTaskDelay(pdMS_TO_TICKS(5));
    }
}
}
bool begin(){
    commands=xQueueCreate(16,sizeof(Request));snapshots=xQueueCreate(1,sizeof(Snapshot));
    if(!commands||!snapshots)return false;
    WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);
    const IPAddress ip(192,168,8,1),mask(255,255,255,0);
    if(!WiFi.softAPConfig(ip,ip,mask)||!WiFi.softAP(IR_LINK_SSID,IR_LINK_PASSWORD,6,false,1))return false;
    do{session=esp_random();}while(!session);
    const char* headers[]={"X-Knob-Key"};server.collectHeaders(headers,1);
    server.on("/ir/poll",HTTP_GET,poll);server.onNotFound([](){server.send(404,"text/plain","Not found");});
    server.begin();return xTaskCreatePinnedToCore(task,"ir_link",6144,nullptr,1,nullptr,0)==pdPASS;
}
bool request(Action a){
    if(!commands||!available.load()){++rejected;return false;}
    Request r{a,millis()};if(xQueueSend(commands,&r,0)!=pdTRUE){++rejected;return false;}return true;
}
void read(Snapshot& s){if(snapshots)xQueuePeek(snapshots,&s,0);}
}
