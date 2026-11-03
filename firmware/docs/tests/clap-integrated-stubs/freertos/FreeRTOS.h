#pragma once
#include <stddef.h>
#include <stdint.h>
using BaseType_t = int;
using TickType_t = uint32_t;
using QueueHandle_t = void*;
constexpr int pdTRUE=1, pdPASS=1;
#define pdMS_TO_TICKS(ms) (ms)
