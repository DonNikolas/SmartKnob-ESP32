// Core-0 display and LED renderer. All hardware control remains on core 1.
#pragma once
#include "model.h"
namespace KnobView {
bool begin();
void publish(const KnobApp::State& state);
}
