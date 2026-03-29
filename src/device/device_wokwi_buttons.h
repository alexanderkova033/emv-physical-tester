#pragma once

#include "device_core.h"

// Input adapter: Wokwi “REST buttons” map GPIO edges to device use-cases + Serial presenter.
// Keeps main.c free of button↔controller wiring.

void device_wokwi_buttons_setup_pinmodes(void);
void device_wokwi_buttons_poll(DeviceController *dc, int default_depth_mm,
                               int default_speed_mm_s);
