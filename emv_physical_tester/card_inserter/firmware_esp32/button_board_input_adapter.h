#pragma once

#include "card_inserter_use_case_controller.h"

// Input adapter: REST-mapped pushbuttons on the button board drive use-cases + Serial output.
// Keeps the app composition root free of button↔controller wiring.

void device_button_board_setup_pinmodes(void);
void device_button_board_poll(DeviceController* dc, int default_depth_mm,
                               int default_speed_mm_s);
void device_button_board_poll_during_motion(DeviceController* dc);

