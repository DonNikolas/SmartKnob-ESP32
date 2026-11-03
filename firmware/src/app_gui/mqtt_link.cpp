// MQTT network operations belong exclusively to a low-priority core-0 task.
// No WiFi mode changes, motor access, flash writes or LVGL calls here.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "mqtt_link.h"
#include "mqtt_config.h"
#include "mqtt_value.h"
namespace KnobMQTT {
namespace {
namespace C=KnobMQTTConfig;
QueueHandle_t incoming=nullptr, temperatures=nullptr;
struct Temperature { float value; bool valid; uint32_t at; };
Snapshot state;
void stateChanged(const char* status) {
    snprintf(state.status,sizeof(state.status),"%s",status);
    ++state.sequence;if(incoming)xQueueOverwrite(incoming,&state);
}
void received(char* topic,uint8_t* payload,unsigned int length) {
    if(strcmp(topic,C::INPUT_TOPIC))return;
    double value=0;
    if(!parseValue(payload,length,value)) {
        state.valid=false;stateChanged("Invalid MQTT value");return;
    }
    state.value=value;state.valid=true;state.receivedAt=millis();stateChanged("MQTT received");
}
bool topicOK(const char* topic) {
    return topic[0] && strlen(topic)<=120 && !strchr(topic,'#') && !strchr(topic,'+');
}
bool configOK() {
    return C::HOST[0] && C::PORT>0 && C::PORT<=65535 && C::PUBLISH_MS>=1000 &&
        C::RECEIVE_MAX_AGE_MS>=1000 && C::RECEIVE_MAX_AGE_MS<=3600000 &&
        C::INPUT_DECIMALS<=3 && (!C::TLS || C::CA_CERT[0]) &&
        topicOK(C::TEMPERATURE_TOPIC)&&topicOK(C::INPUT_TOPIC)&&topicOK(C::STATUS_TOPIC)&&
        strcmp(C::TEMPERATURE_TOPIC,C::INPUT_TOPIC)&&strcmp(C::STATUS_TOPIC,C::INPUT_TOPIC)&&
        strcmp(C::STATUS_TOPIC,C::TEMPERATURE_TOPIC);
}
void task(void*) {
    WiFiClient plain;WiFiClientSecure secure;
    plain.setTimeout(1500);secure.setTimeout(1500);
    if(C::TLS){secure.setCACert(C::CA_CERT);secure.setHandshakeTimeout(3);}
    Client& transport=C::TLS?static_cast<Client&>(secure):static_cast<Client&>(plain);
    PubSubClient mqtt(transport);
    mqtt.setServer(C::HOST,static_cast<uint16_t>(C::PORT));mqtt.setCallback(received);
    mqtt.setSocketTimeout(1);mqtt.setKeepAlive(15);
    if(!mqtt.setBufferSize(512)) {stateChanged("MQTT buffer failed");vTaskDelete(nullptr);return;}
    char id[40];const uint64_t mac=ESP.getEfuseMac();
    snprintf(id,sizeof(id),"smartknob-%04lx%08lx",static_cast<unsigned long>((mac>>32)&0xffff),static_cast<unsigned long>(mac&0xffffffff));
    uint32_t triedAt=0,publishedAt=0;bool tried=false,published=false;
    for(;;) {
        uint32_t now=millis();
        if(WiFi.status()!=WL_CONNECTED || (C::TLS && time(nullptr)<1700000000)) {
            // Close transport without DISCONNECT so the broker can issue LWT.
            transport.stop();
            state.connected=false;state.valid=false;
            const char* reason=WiFi.status()!=WL_CONNECTED?"MQTT: Wi-Fi offline":"MQTT: waiting for clock";
            if(strcmp(state.status,reason))stateChanged(reason);
        } else if(!mqtt.connected()) {
            if(state.connected){state.connected=false;state.valid=false;stateChanged("MQTT disconnected");}
            if(!tried || now-triedAt>=5000) {
                tried=true;triedAt=now;transport.stop();
                // Pre-connect TCP with a bounded timeout. DNS/TLS still run only
                // in this worker; this is not a hard real-time network guarantee.
                bool tcp=C::TLS ? secure.connect(C::HOST,C::PORT) : plain.connect(C::HOST,C::PORT,1500);
                bool ok=tcp && mqtt.connect(id,C::USER[0]?C::USER:nullptr,C::USER[0]?C::PASSWORD:nullptr,
                    C::STATUS_TOPIC,1,true,"offline",true);
                if(ok)ok=mqtt.subscribe(C::INPUT_TOPIC,1)&&mqtt.publish(C::STATUS_TOPIC,"online",true);
                if(ok){state.connected=true;state.valid=false;published=false;stateChanged("MQTT waiting for value");}
                else {transport.stop();state.connected=false;state.valid=false;stateChanged("MQTT connection failed");}
                triedAt=millis();
            }
        } else {
            mqtt.loop();now=millis();
            if(!mqtt.connected()) {
                state.connected=false;state.valid=false;stateChanged("MQTT disconnected");
            } else {
                if(state.valid && !fresh(now,state.receivedAt,C::RECEIVE_MAX_AGE_MS)) {
                    state.valid=false;stateChanged("MQTT value expired");
                }
                if(!published || now-publishedAt>=C::PUBLISH_MS) {
                    Temperature t={};
                    if(xQueuePeek(temperatures,&t,0)==pdTRUE && t.valid && isfinite(t.value) && fresh(now,t.at,6000)) {
                        char payload[32];snprintf(payload,sizeof(payload),"%.2f",static_cast<double>(t.value));
                        // Temperature is not retained: no persistent stale measurement.
                        if(mqtt.publish(C::TEMPERATURE_TOPIC,payload,false)) {publishedAt=now;published=true;}
                        else {transport.stop();state.connected=false;state.valid=false;stateChanged("MQTT publish failed");}
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
}
void begin() {
    if(incoming)return;
    incoming=xQueueCreate(1,sizeof(Snapshot));temperatures=xQueueCreate(1,sizeof(Temperature));
    if(!incoming||!temperatures){stateChanged("MQTT queue failed");return;}
    if(!KnobMQTTConfig::ENABLED){stateChanged("MQTT disabled");return;}
    if(!configOK()){stateChanged("MQTT config incomplete");return;}
    stateChanged("MQTT starting");
    if(xTaskCreatePinnedToCore(task,"mqtt",8192,nullptr,1,nullptr,0)!=pdPASS)stateChanged("MQTT worker failed");
}
void temperature(float value,bool valid,uint32_t measuredAt) {
    if(temperatures){Temperature t={value,valid,measuredAt};xQueueOverwrite(temperatures,&t);}
}
void read(Snapshot& out) {
    if(incoming)xQueuePeek(incoming,&out,0);
    else {out.valid=false;out.connected=false;snprintf(out.status,sizeof(out.status),"MQTT unavailable");}
}
}
