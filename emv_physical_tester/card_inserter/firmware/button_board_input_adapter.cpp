#include "button_board_input_adapter.h"

#include <Arduino.h>

#include "button_board_pins.h"
#include "arduino_device_adapter.h"

namespace {

struct ButtonPrevState {
  int insert = HIGH;
  int home = HIGH;
  int remove = HIGH;
  int status = HIGH;
  int abort = HIGH;
};

ButtonPrevState g_prev{};

inline bool RisingEdgePressed(int current_level, int previous_level) {
  return (current_level == LOW) && (previous_level == HIGH);
}

inline bool EstopAsserted() { return digitalRead(PIN_ESTOP) == LOW; }

}  // namespace

void device_button_board_setup_pinmodes(void) {
  pinMode(PIN_INSERT, INPUT_PULLUP);
  pinMode(PIN_HOME, INPUT_PULLUP);
  pinMode(PIN_REMOVE, INPUT_PULLUP);
  pinMode(PIN_STATUS, INPUT_PULLUP);
  pinMode(PIN_ABORT, INPUT_PULLUP);
  pinMode(PIN_ESTOP, INPUT_PULLUP);
}

void device_button_board_poll(DeviceController *dc, int default_depth_mm,
                               int default_speed_mm_s) {
  const int ins = digitalRead(PIN_INSERT);
  const int hom = digitalRead(PIN_HOME);
  const int rem = digitalRead(PIN_REMOVE);
  const int st = digitalRead(PIN_STATUS);
  const int ab = digitalRead(PIN_ABORT);

  if (RisingEdgePressed(st, g_prev.status)) {
    device_serial_log_cmd("GET /api/status");
    DeviceStatus s = dc->GetStatus();
    device_serial_print_status(&s);
  }

  if (RisingEdgePressed(ab, g_prev.abort)) {
    device_serial_log_cmd("POST /api/abort");
    const DeviceStatus s = dc->GetStatus();
    if (s.state != ST_INSERTING && s.state != ST_REMOVING && s.state != ST_HOMING) {
      Serial.println(F("[CMD]        (ignored — not in HOMING / INSERTING / REMOVING)"));
    } else {
      dc->Abort();
      Serial.println(F("[CMD]        motion stop requested"));
    }
  }

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
  g_prev.home = hom;
  g_prev.remove = rem;
  g_prev.status = st;
  g_prev.abort = ab;
}
