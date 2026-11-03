// Executes the real background.cpp worker with deterministic peripheral mocks.
// This checks integration/control flow, NOT ESP32 timing or driver compatibility.
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>
#include <memory>
#include <iostream>
#include "../../src/app_gui/background.cpp"
uint32_t fakeMs=100;
FakeWire Wire;
namespace KnobMQTT { void begin() {} void temperature(float,bool,uint32_t) {} }
namespace {
struct Done {};
struct Queue { size_t bytes; std::deque<std::vector<unsigned char>> items; };
std::vector<std::unique_ptr<Queue>> queues;
unsigned reads=0,limit=0,mode=0,uninstalled=0;
bool taskFail=false;
Queue* q(QueueHandle_t h) { assert(h);return static_cast<Queue*>(h); }
}
QueueHandle_t xQueueCreate(unsigned,size_t n) {
    queues.emplace_back(new Queue{n,{}}); return queues.back().get();
}
BaseType_t xQueueOverwrite(QueueHandle_t h,const void* p) {
    auto a=q(h);a->items.clear();const auto b=static_cast<const unsigned char*>(p);
    a->items.emplace_back(b,b+a->bytes);return pdTRUE;
}
BaseType_t xQueuePeek(QueueHandle_t h,void* p,TickType_t) {
    auto a=q(h);if(a->items.empty())return 0;
    std::memcpy(p,a->items.front().data(),a->bytes);return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t h,void* p,TickType_t t) {
    if(!xQueuePeek(h,p,t))return 0;
    q(h)->items.pop_front();return pdTRUE;
}
BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,uint32_t,void*,int,void*,int) {
    return taskFail?0:pdPASS;
}
void vTaskDelay(TickType_t t) {fakeMs+=t;}
void vTaskDelete(void*) {throw Done{};}
esp_err_t i2s_driver_install(i2s_port_t,const i2s_config_t* c,int,void* events) {
    assert(c->sample_rate==16000 && c->bits_per_sample==16 && c->dma_buf_len==256);
    if(mode==1)return -1;
    *static_cast<QueueHandle_t*>(events)=xQueueCreate(16,sizeof(i2s_event_t));return ESP_OK;
}
esp_err_t i2s_set_pin(i2s_port_t,const i2s_pin_config_t* p) {
    assert(p->ws_io_num==38 && p->data_in_num==39);return mode==2?-1:ESP_OK;
}
esp_err_t i2s_set_pdm_rx_down_sample(i2s_port_t,int n) {assert(n==16);return ESP_OK;}
esp_err_t i2s_driver_uninstall(i2s_port_t) {++uninstalled;return ESP_OK;}
esp_err_t i2s_read(i2s_port_t,void* dest,size_t n,size_t* got,TickType_t) {
    if(reads++>=limit)throw Done{};
    fakeMs+=16;
    if(mode==3 && reads==160) {
        i2s_event_t e{I2S_EVENT_RX_Q_OVF,0};xQueueOverwrite(KnobBackground::i2sEvents,&e);
    }
    if(mode==4 && reads==160)fakeMs+=100;
    if(mode==5 && reads==160){*got=0;return -1;}
    if(mode==6 && reads==160)KnobBackground::suppressClick();
    *got=n;
    auto p=static_cast<int16_t*>(dest);
    for(unsigned i=0;i<n/sizeof(int16_t);++i) {
        // Non-flat continuous background, or a completely flat receiver.
        p[i]=mode==7?0:static_cast<int16_t>(20*sinf((i/2)*.4f));
    }
    return ESP_OK;
}
KnobBackground::Audio run(unsigned m,unsigned n) {
    using namespace KnobBackground;
    queues.clear();fakeMs=100;reads=0;limit=n;mode=m;uninstalled=0;
    suppressionEpoch.store(0);suppressionAt.store(0);stream.reset();analyzer.reset();
    begin();try{audioTask(nullptr);}catch(const Done&){}
    Audio a;Environment e;read(e,a);return a;
}
int main() {
    auto a=run(0,160);assert(!std::strcmp(a.status,"Clap: ready") && a.frames==160 && !a.pairs);
    a=run(1,0);assert(!std::strcmp(a.status,"Mic: init failed") && a.errors==1 && !uninstalled);
    a=run(2,0);assert(!std::strcmp(a.status,"Mic: init failed") && a.errors==1 && uninstalled==1);
    for(unsigned mode=3;mode<=5;++mode){a=run(mode,170);assert(a.errors==1 && !a.pairs && std::strcmp(a.status,"Clap: ready"));}
    a=run(6,165);assert(!std::strcmp(a.status,"Clap: haptic pause") && !a.pairs);
    a=run(7,200);assert(!std::strcmp(a.status,"Mic: no signal") && !a.pairs);
    a=run(0,160);fakeMs+=501;KnobBackground::Environment e;KnobBackground::read(e,a);
    assert(!std::strcmp(a.status,"Mic: data stale"));
    KnobBackground::suppressClick();assert(KnobBackground::clickSuppressed(fakeMs));
    fakeMs+=300;assert(!KnobBackground::clickSuppressed(fakeMs));
    taskFail=true;KnobBackground::begin();a={};KnobBackground::read(e,a);
    assert(!std::strcmp(a.status,"Mic: task failed"));
    std::cout<<"PASS real audio worker with mocks: ready, init failure, overflow, timing gap, read error, suppression, flat/stale input, task failure\n";
}
