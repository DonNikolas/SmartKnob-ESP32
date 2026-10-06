#pragma once
#include <stdint.h>
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 2
#define OUTPUT 1
uint32_t millis();
int digitalRead(int pin);
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution);
bool ledcWrite(uint8_t pin, uint32_t duty);
void configTzTime(const char *tz, const char *server1, const char *server2, const char *server3 = nullptr);
struct HostSerial {
    void begin(unsigned long) {}
    void println(const char *) {}
};
extern HostSerial Serial;
void delay(unsigned long ms);

// Arduino-ESP32 definitions from the reported 3.3.7 compiler diagnostics.
// Keep them in the host test to catch collisions with sketch identifiers.
#define PI 3.1415926535897932384626433832795
#define DISPLAY 0x1
typedef enum { HOST_ETS_OK, HOST_ETS_FAILED } STATUS;
