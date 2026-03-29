#include <Servo.h>
#include "device_board_pins.h"
#include "device_core.h"
#include "device_arduino_presenter.h"
#include "device_wokwi_buttons.h"

// >0 = type error messages one character at a time (Serial Monitor); 0 = print instantly.
#ifndef DEBUG_ERR_CHAR_MS
#define DEBUG_ERR_CHAR_MS 12
#endif

Servo carriage;

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

 device_wokwi_buttons_setup_pinmodes();

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
 device_wokwi_buttons_poll(&g_dc, DEFAULT_DEPTH_MM, DEFAULT_SPEED_MM_S);
}
