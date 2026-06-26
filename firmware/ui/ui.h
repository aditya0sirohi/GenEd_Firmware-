#pragma once
#include "../hal/include/hal.h"

// UI Task — drives LEDs, buzzer based on device state
// Consumed by UiTask in app_runtime/main.cpp
void UiTask_run(IIO* io);
