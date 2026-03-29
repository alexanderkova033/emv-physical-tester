#include "wokwi_buttons.h"

#include <Arduino.h>

#include "../arduino/arduino_board_pins.h"
#include "../arduino/arduino_presenter.h"

static int s_prevInsert = HIGH;
static int s_prevHome = HIGH;
static int s_prevRemove = HIGH;
static int s_prevStatus = HIGH;
static int s_prevAbort = HIGH;
static int s_prevReset = HIGH;
static int s_prevEvents = HIGH;
static int s_prevReserve = HIGH;
static int s_prevRelease = HIGH;

static bool estop_asserted(void) { return digitalRead(PIN_ESTOP) == LOW; }

void device_wokwi_buttons_setup_pinmodes(void) {
  pinMode(PIN_INSERT, INPUT_PULLUP);
  pinMode(PIN_HOME, INPUT_PULLUP);
  pinMode(PIN_REMOVE, INPUT_PULLUP);
  pinMode(PIN_STATUS, INPUT_PULLUP);
  pinMode(PIN_ABORT, INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP);
  pinMode(PIN_EVENTS, INPUT_PULLUP);
  pinMode(PIN_RESERVE, INPUT_PULLUP);
  pinMode(PIN_RELEASE, INPUT_PULLUP);
  pinMode(PIN_ESTOP, INPUT_PULLUP);
}

void device_wokwi_buttons_poll(DeviceController *dc, int default_depth_mm,
                               int default_speed_mm_s) {
  const int ins = digitalRead(PIN_INSERT);
  const int hom = digitalRead(PIN_HOME);
  const int rem = digitalRead(PIN_REMOVE);
  const int st = digitalRead(PIN_STATUS);
  const int ab = digitalRead(PIN_ABORT);
  const int rst = digitalRead(PIN_RESET);
  const int ev = digitalRead(PIN_EVENTS);
  const int res = digitalRead(PIN_RESERVE);
  const int rel = digitalRead(PIN_RELEASE);

  if (st == LOW && s_prevStatus == HIGH) {
    device_serial_log_cmd("GET /api/status");
    DeviceStatus s = device_get_status(dc);
    device_serial_print_status(&s);
  }
  if (ev == LOW && s_prevEvents == HIGH) {
    device_serial_log_cmd("GET /api/events (last STATE_CHANGED)");
    DeviceStatus s = device_get_status(dc);
    device_serial_print_last_event(s.last_evt_old, s.last_evt_new);
  }
  if (res == LOW && s_prevReserve == HIGH) {
    device_serial_log_cmd("POST /api/reserve");
    device_api_reserve(dc);
  }
  if (rel == LOW && s_prevRelease == HIGH) {
    device_serial_log_cmd("POST /api/release");
    device_api_release(dc);
  }

  if (ab == LOW && s_prevAbort == HIGH) {
    device_serial_log_cmd("POST /api/abort");
    const DeviceStatus s = device_get_status(dc);
    if (s.state != ST_INSERTING && s.state != ST_REMOVING && s.state != ST_HOMING) {
      Serial.println(F("[CMD]        (ignored — not in HOMING / INSERTING / REMOVING)"));
    } else {
      device_api_abort(dc);
      Serial.println(F("[CMD]        motion stop requested"));
    }
  }
  if (rst == LOW && s_prevReset == HIGH) {
    device_serial_log_cmd("POST /api/reset");
    device_api_reset(dc);
  }

  if (!estop_asserted()) {
    if (ins == LOW && s_prevInsert == HIGH) {
      device_serial_log_cmd("POST /api/insert");
      device_api_insert(dc, default_depth_mm, default_speed_mm_s);
    }
    if (hom == LOW && s_prevHome == HIGH) {
      device_serial_log_cmd("POST /api/home");
      device_api_home(dc);
    }
    if (rem == LOW && s_prevRemove == HIGH) {
      device_serial_log_cmd("POST /api/remove");
      device_api_remove(dc);
    }
  }

  s_prevInsert = ins;
  s_prevHome = hom;
  s_prevRemove = rem;
  s_prevStatus = st;
  s_prevAbort = ab;
  s_prevReset = rst;
  s_prevEvents = ev;
  s_prevReserve = res;
  s_prevRelease = rel;
}

