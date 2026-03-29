#include <Servo.h>
#include "device_core.h"
#include "device_arduino_presenter.h"

// >0 = type error messages one character at a time (Serial Monitor); 0 = print instantly.
#ifndef DEBUG_ERR_CHAR_MS
#define DEBUG_ERR_CHAR_MS 12
#endif

// Wokwi emulator: physical mapping for protocol-and-api-spec.md
//   POST /api/insert     -> D2 (blue)
//   POST /api/home       -> D3 (green)
//   POST /api/remove     -> D4 (yellow)
//   GET  /api/status     -> D5 (white) — prints JSON on Serial
//   POST /api/abort      -> D6 (red)
//   POST /api/reset      -> D7 (green)
//   GET  /api/events *   -> D8 (black) — prints last STATE_CHANGED line on Serial
//   POST /api/reserve *  -> D9 (blue)
//   POST /api/release *  -> D11 (yellow)
//   E-stop input         -> D12 (red); LOW = asserted (503 / ESTOP_ASSERTED)
// * Optional reservation endpoints per spec; SERIAL output mimics SSE "data:" lines.
// Servo PWM: D10

Servo carriage;

const int PIN_INSERT = 2;
const int PIN_HOME = 3;
const int PIN_REMOVE = 4;
const int PIN_STATUS = 5;
const int PIN_ABORT = 6;
const int PIN_RESET = 7;
const int PIN_EVENTS = 8;
const int PIN_RESERVE = 9;
const int PIN_RELEASE = 11;
const int PIN_ESTOP = 12;
const int PIN_SERVO_PWM = 10;

// Servo angles (degrees): full retract reference vs. retract-after-remove.
const int ANGLE_HOME = 0;
const int ANGLE_REMOVE = 30;
const int ANGLE_INSERT = 152;
const int MAX_DEPTH_MM = 50;
const int DEFAULT_DEPTH_MM = 35;
const int DEFAULT_SPEED_MM_S = 20;

typedef struct ArduinoDeviceCtx {
 uint16_t err_char_ms;
} ArduinoDeviceCtx;

static ArduinoDeviceCtx g_ctx = { DEBUG_ERR_CHAR_MS };
static DeviceController g_dc;

static int prevInsert = HIGH;
static int prevHome = HIGH;
static int prevRemove = HIGH;
static int prevStatus = HIGH;
static int prevAbort = HIGH;
static int prevReset = HIGH;
static int prevEvents = HIGH;
static int prevReserve = HIGH;
static int prevRelease = HIGH;

static bool port_estop_asserted(void *ctx) {
 (void)ctx;
 return digitalRead(PIN_ESTOP) == LOW;
}

static void port_servo_write(void *ctx, int angle) {
 (void)ctx;
 carriage.write(angle);
}

static void port_delay_ms(void *ctx, uint16_t ms) {
 (void)ctx;
 delay(ms);
}

static uint32_t port_now_ms(void *ctx) {
 (void)ctx;
 return (uint32_t)millis();
}

static void port_emit_state_changed(void *ctx, DeviceState old_s, DeviceState new_s) {
 (void)ctx;
 device_serial_emit_state_changed(old_s, new_s);
}

static void port_emit_reservation(void *ctx, bool acquired) {
 (void)ctx;
 device_serial_emit_reservation(acquired);
}

static void port_log_cmd(void *ctx, const char *line) {
 (void)ctx;
 device_serial_log_cmd(line);
}

static void port_log_ok(void *ctx, const char *line) {
 (void)ctx;
 device_serial_log_ok(line);
}

static void port_log_err(void *ctx, ErrCode e, DeviceState current_state,
                         const char *command_label, const char *detail_override) {
 ArduinoDeviceCtx *c = (ArduinoDeviceCtx *)ctx;
 device_serial_log_err_typed(e, current_state, command_label, detail_override,
                             c ? c->err_char_ms : 0);
}

void setup() {
 Serial.begin(9600);

 carriage.attach(PIN_SERVO_PWM);
 carriage.write(ANGLE_HOME);

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

 DeviceConfig cfg;
 cfg.angle_home = ANGLE_HOME;
 cfg.angle_remove = ANGLE_REMOVE;
 cfg.angle_insert = ANGLE_INSERT;
 cfg.max_depth_mm = MAX_DEPTH_MM;
 cfg.default_depth_mm = DEFAULT_DEPTH_MM;
 cfg.default_speed_mm_s = DEFAULT_SPEED_MM_S;

 DevicePorts ports;
 ports.ctx = &g_ctx;
 ports.estop_asserted = port_estop_asserted;
 ports.servo_write_angle = port_servo_write;
 ports.delay_ms = port_delay_ms;
 ports.now_ms = port_now_ms;
 ports.emit_state_changed = port_emit_state_changed;
 ports.emit_reservation = port_emit_reservation;
 ports.log_cmd = port_log_cmd;
 ports.log_ok = port_log_ok;
 ports.log_err = port_log_err;

 device_init(&g_dc, &cfg, &ports);

 // If E-stop is already asserted at boot, enter ERROR immediately.
 device_on_estop(&g_dc);

 Serial.println(F("// Buttons ~ REST: INSERT HOME REMOVE ABORT RESET; STATUS EVENTS; RESERVE RELEASE; E-STOP"));
}

void loop() {
 device_on_estop(&g_dc);

 int ins = digitalRead(PIN_INSERT);
 int hom = digitalRead(PIN_HOME);
 int rem = digitalRead(PIN_REMOVE);
 int st = digitalRead(PIN_STATUS);
 int ab = digitalRead(PIN_ABORT);
 int rst = digitalRead(PIN_RESET);
 int ev = digitalRead(PIN_EVENTS);
 int res = digitalRead(PIN_RESERVE);
 int rel = digitalRead(PIN_RELEASE);

 if (st == LOW && prevStatus == HIGH) {
  device_serial_log_cmd("GET /api/status");
  DeviceStatus s = device_get_status(&g_dc);
  device_serial_print_status(&s);
 }
 if (ev == LOW && prevEvents == HIGH) {
  device_serial_log_cmd("GET /api/events (last STATE_CHANGED)");
  DeviceStatus s = device_get_status(&g_dc);
  device_serial_print_last_event(s.last_evt_old, s.last_evt_new);
 }
 if (res == LOW && prevReserve == HIGH) {
  device_serial_log_cmd("POST /api/reserve");
  device_api_reserve(&g_dc);
 }
 if (rel == LOW && prevRelease == HIGH) {
  device_serial_log_cmd("POST /api/release");
  device_api_release(&g_dc);
 }

 if (ab == LOW && prevAbort == HIGH) {
  device_serial_log_cmd("POST /api/abort");
  const DeviceStatus s = device_get_status(&g_dc);
  if (s.state != ST_INSERTING && s.state != ST_REMOVING && s.state != ST_HOMING) {
   Serial.println(F("[CMD]        (ignored — not in HOMING / INSERTING / REMOVING)"));
  } else {
   device_api_abort(&g_dc);
   Serial.println(F("[CMD]        motion stop requested"));
  }
 }
 if (rst == LOW && prevReset == HIGH) {
  device_serial_log_cmd("POST /api/reset");
  device_api_reset(&g_dc);
 }

 if (!port_estop_asserted(nullptr)) {
  if (ins == LOW && prevInsert == HIGH) {
   device_serial_log_cmd("POST /api/insert");
   device_api_insert(&g_dc, DEFAULT_DEPTH_MM, DEFAULT_SPEED_MM_S);
  }
  if (hom == LOW && prevHome == HIGH) {
   device_serial_log_cmd("POST /api/home");
   device_api_home(&g_dc);
  }
  if (rem == LOW && prevRemove == HIGH) {
   device_serial_log_cmd("POST /api/remove");
   device_api_remove(&g_dc);
  }
 }

 prevInsert = ins;
 prevHome = hom;
 prevRemove = rem;
 prevStatus = st;
 prevAbort = ab;
 prevReset = rst;
 prevEvents = ev;
 prevReserve = res;
 prevRelease = rel;
}
