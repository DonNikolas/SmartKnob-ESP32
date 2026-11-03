#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,uint32_t,void*,int,void*,int);
void vTaskDelay(TickType_t);
void vTaskDelete(void*);
