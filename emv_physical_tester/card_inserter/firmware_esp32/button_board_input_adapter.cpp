#include "button_board_input_adapter.h"

#include <Arduino.h>

#include "esp32_pins.h"
#include "esp32_adapter.h"

namespace {

struct ButtonPrevState {
  int insert = HIGH;
  int home   = HIGH;
  int remove = HIGH;
  int status = HIGH;
  int abort  = HIGH;
};

ButtonPrevState g_prev{};

inline bool RisingEdgePressed(int current_level, int previous_level) {
  return (current_level == LOW) && (previous_level == HIGH);
}

inline bool EstopAsserted() { return digitalRead(PIN_ESTOP) == LOW; }

void PollStatusAbortEdges(DeviceController* dc, int st, int ab) {
  if (RisingEdgePressed(st, g_prev.status)) {
    device_serial_log_cmd("GET /api/status");
    DeviceStatus s = dc->GetStatus();
    device_serial_print_status(&s);
  }

  if (RisingEdgePressed(ab, g_prev.abort)) {
    device_serial_log_cmd("POST /api/abort");
    const DeviceStatus s = dc->GetStatus();
    if (s.state != ST_INSERTING && s.state != ST_REMOVING && s.state != ST_HOMING) {
      Serial.println(F("[CMD]        (ignored — not in motion)"));
    } else {
      dc->Abort();
      Serial.println(F("[CMD]        motion stop requested"));
    }
  }

  g_prev.status = st;
  g_prev.abort  = ab;
}

}  // namespace

void device_button_board_setup_pinmodes(void) {
  // Pin modes are already set in esp32_hw_init(); this is a no-op here.
}

void device_button_board_poll_during_motion(DeviceController* dc) {
  dc->OnEstop();
  const int st = digitalRead(PIN_STATUS);
  const int ab = digitalRead(PIN_ABORT);
  PollStatusAbortEdges(dc, st, ab);
}

void device_button_board_poll(DeviceController* dc, int default_depth_mm,
                               int default_speed_mm_s) {
  const int ins = digitalRead(PIN_INSERT);
  const int hom = digitalRead(PIN_HOME);
  const int rem = digitalRead(PIN_REMOVE);
  const int st  = digitalRead(PIN_STATUS);
  const int ab  = digitalRead(PIN_ABORT);
  PollStatusAbortEdges(dc, st, ab);

  if (!EstopAsserted()) {
    if (RisingEdgePressed(ins, g_prev.insert)) {
      device_serial_log_cmd("POST /api/insert");
      dc->Insert(default_depth_mm, default_speed_mm_s);
    }
    if (RisingEdgePressed(hom, g_prev.home)) {
      device_serial_log_cmd("POST /api/home");
      dc->Home();
    }
    if (RisingEdgePressed(rem, g_prev.remove)) {
      device_serial_log_cmd("POST /api/remove");
      dc->Remove();
    }
  }

  g_prev.insert = ins;
  g_prev.home   = hom;
  g_prev.remove = rem;
}
