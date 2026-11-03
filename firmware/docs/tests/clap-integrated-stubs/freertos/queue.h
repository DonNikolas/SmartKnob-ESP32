#pragma once
#include "FreeRTOS.h"
QueueHandle_t xQueueCreate(unsigned,size_t);
BaseType_t xQueueOverwrite(QueueHandle_t,const void*);
BaseType_t xQueueReceive(QueueHandle_t,void*,TickType_t);
BaseType_t xQueuePeek(QueueHandle_t,void*,TickType_t);
