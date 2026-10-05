#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("SmartKnob firmware started.");
}

void loop()
{
    Serial.println("SmartKnob is running.");
    delay(2000);
}