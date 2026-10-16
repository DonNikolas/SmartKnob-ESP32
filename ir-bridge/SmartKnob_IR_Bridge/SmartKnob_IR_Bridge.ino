// ESP8266 companion for SmartKnob. Generated with ChatGPT assistance.
// Based on the user's working Receive-IR sketch: GPIO14, NEC, 32 bits.
// Arduino libraries: IRremoteESP8266 2.8.6 and ArduinoJson 6.21.5.
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "config_private.h"
#include "receiver_state.h"
extern "C" {
#include <osapi.h>
}
IRsend transmitter(14);
uint32_t bootId=0,lastPoll=0;
ReceiverState receiver;
void setup(){
    Serial.begin(115200);transmitter.begin();
    do{bootId=os_random();}while(!bootId);
    WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);
    WiFi.begin(IR_LINK_SSID,IR_LINK_PASSWORD);
    Serial.println("SmartKnob IR bridge: connecting. GPIO14 / NEC32.");
}
void loop(){
    if(WiFi.status()!=WL_CONNECTED){delay(20);return;}
    if(millis()-lastPoll<140){delay(2);return;}lastPoll=millis();
    WiFiClient client;HTTPClient http;
    String url=String("http://192.168.8.1/ir/poll?boot=")+bootId+"&session="+receiver.session+"&ack="+receiver.executed;
    if(!http.begin(client,url))return;
    http.setTimeout(400);http.addHeader("X-Knob-Key",IR_LINK_KEY);
    int status=http.GET();
    String body=status==200?http.getString():String();http.end();
    // A delayed reply must not cause a stale relative brightness operation.
    if(status!=200||body.length()>160||millis()-lastPoll>600)return;
    StaticJsonDocument<192> doc;
    if(deserializeJson(doc,body)||!doc["session"].is<uint32_t>()||!doc["id"].is<uint32_t>()||!doc["cmd"].is<const char*>())return;
    uint32_t session=doc["session"],id=doc["id"];
    if(!receiver.accept(session,id))return;
    const char* cmd=doc["cmd"];uint32_t code=0;
    if(!strcmp(cmd,"on"))code=0x00F7C03F;
    else if(!strcmp(cmd,"off"))code=0x00F740BF;
    else if(!strcmp(cmd,"up"))code=0x00F700FF;
    else if(!strcmp(cmd,"down"))code=0x00F7807F;
    else return;
    transmitter.sendNEC(code,32);
    receiver.sent(id); // Retained in RAM across Wi-Fi reconnects, not across reboot.
    Serial.printf("IR sent: %s | id: %lu\n",cmd,(unsigned long)id);
}
