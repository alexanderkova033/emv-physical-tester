#pragma once

#include "../../use_cases/device_controller.h"

// Input adapter: Wokwi “REST buttons” map GPIO edges to device use-cases + Serial presenter.
// Keeps the app composition root free of button↔controller wiring.

void device_wokwi_buttons_setup_pinmodes(void);
void device_wokwi_buttons_poll(DeviceController *dc, int default_depth_mm,
                               int default_speed_mm_s);

