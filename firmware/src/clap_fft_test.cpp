// Standalone audio diagnostic. Motor, backlight and LED data stay LOW.
// Uses the same PDM reception setup as the supplied clap_detection.cpp.
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "clap_fft_features.h"
#include "clap_fft_detector.h"

namespace {
constexpr i2s_port_t PORT=I2S_NUM_0;
constexpr uint8_t MOTOR_PINS[]={4,7,5,16,6,15};
int16_t stereo[ClapFFT::N*2], mono[ClapFFT::N];
ClapFFT::Analyzer analyzer;
ClapFFT::Detector detector;
QueueHandle_t logQueue=nullptr;
uint32_t sequence=0, dropped=0, errors=0, calibration=0;
double calibrationSum=0;
float background=0, previousLevel=0;
unsigned phase=0, slot=0;
bool stopped=false;
size_t filled=0;
struct Record {
    ClapFFT::Features f;
    uint32_t seq,ms,drops,readErrors;
    float other,background,trigger,rise;
    unsigned phase,slot,kind,impulse,calibrating;
    uint32_t duration,gap;
};
const char* phaseName(unsigned n) {
    const char* names[]={"QUIET","SINGLE_CLAP","DOUBLE_CLAP","SPEECH","CASE_RUB","TURN","CASE_TAP"};
    return n<7?names[n]:"UNKNOWN";
}
void logger(void*) {
    Record r;
    for(;;) {
        if(xQueueReceive(logQueue,&r,portMAX_DELAY)!=pdTRUE) continue;
        if(r.kind>=6) {
            Serial.printf("# %s audio_ms=%lu duration_ms=%lu pair_gap_ms=%lu phase=%u drops=%lu read_errors=%lu\n",
                r.kind==6?"CLAP_CANDIDATE":r.kind==7?"DOUBLE_CLAP":"REJECTED",
                (unsigned long)(r.seq*16UL),(unsigned long)r.duration,(unsigned long)r.gap,r.phase,
                (unsigned long)r.drops,(unsigned long)r.readErrors);
        } else if(r.kind) {
            Serial.printf("# %s phase=%u:%s slot=%c seq=%lu drops=%lu read_errors=%lu\n",
                r.kind==1?"MARK":r.kind==2?"RECALIBRATE":r.kind==3?"STOP":r.kind==4?"READ_ERROR":"READY",
                r.phase,phaseName(r.phase),r.slot?'B':'A',
                static_cast<unsigned long>(r.seq),static_cast<unsigned long>(r.drops),static_cast<unsigned long>(r.readErrors));
        } else {
            Serial.printf("%lu,%lu,%u,%u,%u,%.1f,%.1f,%.1f,%.1f,%.2f,%.4f,%.4f,%.4f,%.0f,%.4f,%.4f,%.1f,%.1f,%u,%u,%lu,%lu\n",
                static_cast<unsigned long>(r.seq),static_cast<unsigned long>(r.ms),r.phase,r.slot,r.calibrating,
                r.f.rms,r.other,r.f.peak,r.background,r.f.crest,r.f.low,r.f.mid,r.f.high,
                r.f.centroid,r.f.flatness,r.f.flux,r.trigger,r.rise,r.impulse,r.f.clipped,
                static_cast<unsigned long>(r.drops),static_cast<unsigned long>(r.readErrors));
        }
    }
}
void enqueue(Record r) {
    r.seq=sequence;r.ms=millis();r.phase=phase;r.slot=slot;r.drops=dropped;r.readErrors=errors;
    if(xQueueSend(logQueue,&r,0)!=pdTRUE) ++dropped;
}
void mark(unsigned kind) { Record r={};r.kind=kind;enqueue(r); }
void resetAnalysis() {
    calibration=0;calibrationSum=0;background=previousLevel=0;analyzer.reset();detector.reset();
}
float otherRms() {
    double sum=0,square=0;
    for(unsigned i=0;i<ClapFFT::N;++i) {
        const double v=stereo[2*i+(1-slot)];sum+=v;square+=v*v;
    }
    const double mean=sum/ClapFFT::N;
    return sqrt(fmax(0.0,square/ClapFFT::N-mean*mean));
}
void check(esp_err_t e,const char* action) {
    if(e==ESP_OK)return;
    Serial.printf("FAIL: %s: %s\n",action,esp_err_to_name(e));
    while(true)delay(1000);
}
}

void setup() {
    for(uint8_t p:MOTOR_PINS){pinMode(p,OUTPUT);digitalWrite(p,LOW);}
    pinMode(9,OUTPUT);digitalWrite(9,LOW);
    Adafruit_NeoPixel ring(36,11,NEO_GRB+NEO_KHZ800);
    ring.begin();ring.clear();ring.show();
    pinMode(40,OUTPUT);digitalWrite(40,LOW);
    Serial.begin(115200);delay(3000);
    Serial.println("\n# SMARTKNOB FFT DIAGNOSTIC v2 - MOTOR OFF - NO LAMP COMMANDS");
    Serial.println("# 0 quiet | 1 single clap | 2 double clap | 3 speech | 4 case rub | 5 turn | 6 case tap");
    Serial.println("# c recalibrate (quiet 2s) | a/b receive slot | i mark | x stop (reset to restart)");
    Serial.println("# phase labels describe YOUR action; impulse=1 is NOT a confirmed clap.");
    Serial.println("# Experimental: RMS>=max(1500,6*background), rise>=4, mid>=50%, low<=25%, high<=40%, crest>=3.");
    Serial.println("# Decay within 112ms; pair gap 160..600ms; cooldown 700ms. Motor OFF, no lamp commands.");
    Serial.println("# CSV every 256 samples (16ms nominal). Frequency resolution 62.5Hz. Slot A default.");
    Serial.println("seq,ms,phase,slot,cal,rms,other_rms,peak,background,crest,low,mid,high,centroid,flatness,flux,trigger,rise,impulse,clipped,drops,read_errors");
    i2s_config_t c={};
    c.mode=static_cast<i2s_mode_t>(I2S_MODE_MASTER|I2S_MODE_RX|I2S_MODE_PDM);
    c.sample_rate=16000;c.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
    c.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;c.communication_format=I2S_COMM_FORMAT_STAND_I2S;
    c.dma_buf_count=8;c.dma_buf_len=ClapFFT::N;c.use_apll=false;
    check(i2s_driver_install(PORT,&c,0,nullptr),"PDM driver");
    i2s_pin_config_t pins={};
    pins.mck_io_num=I2S_PIN_NO_CHANGE;pins.bck_io_num=I2S_PIN_NO_CHANGE;
    pins.ws_io_num=38;pins.data_out_num=I2S_PIN_NO_CHANGE;pins.data_in_num=39;
    check(i2s_set_pin(PORT,&pins),"PDM pins");
    check(i2s_set_pdm_rx_down_sample(PORT,I2S_PDM_DSR_16S),"PDM conversion");
    logQueue=xQueueCreate(128,sizeof(Record));
    if(!logQueue || xTaskCreatePinnedToCore(logger,"fft-log",4096,nullptr,1,nullptr,0)!=pdPASS) {
        i2s_stop(PORT);Serial.println("FAIL: logger allocation");while(true)delay(1000);
    }
}
void loop() {
    if(stopped){delay(10);return;}
    while(Serial.available()) {
        const char c=Serial.read();
        if(c>='0'&&c<='6'){phase=c-'0';detector.reset();mark(1);}
        else if(c=='i')mark(1);
        else if(c=='c'||c=='a'||c=='b') {
            if(c=='a')slot=0;
            if(c=='b')slot=1;
            resetAnalysis();mark(2);
        } else if(c=='x') {i2s_stop(PORT);stopped=true;mark(3);return;}
    }
    size_t got=0;
    const esp_err_t result=i2s_read(PORT,reinterpret_cast<uint8_t*>(stereo)+filled,
        sizeof(stereo)-filled,&got,pdMS_TO_TICKS(1000));
    if(result!=ESP_OK||got==0||got%4!=0) {
        ++errors;filled=0;resetAnalysis();mark(4);return;
    }
    filled+=got;
    if(filled<sizeof(stereo))return;
    filled=0;++sequence;
    // Discard the first 20 full buffers for filter startup (~320ms).
    if(sequence<=20)return;
    for(unsigned i=0;i<ClapFFT::N;++i)mono[i]=stereo[2*i+slot];
    Record r={};r.f=analyzer.analyze(mono);r.other=otherRms();
    r.calibrating=calibration<125;
    r.background=background;r.trigger=fmaxf(600,4*background);
    r.rise=r.f.rms/fmaxf(1,previousLevel);
    r.impulse=!r.calibrating && r.f.rms>=r.trigger && r.rise>=1.8f;
    if(r.calibrating) {
        calibrationSum+=r.f.rms;++calibration;
        if(calibration==125){background=calibrationSum/125;mark(5);}
    } else if(r.f.rms<r.trigger) background+=0.01f*(r.f.rms-background);
    if(!r.calibrating){
        const auto detection=detector.update(r.f,r.background,r.rise,sequence*16UL);
        if(detection.event){Record event={};event.kind=5+detection.event;event.duration=detection.duration;event.gap=detection.gap;enqueue(event);}
    }
    previousLevel=r.f.rms;
    enqueue(r);
}
