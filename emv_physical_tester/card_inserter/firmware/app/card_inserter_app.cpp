#include "card_inserter_app.h"

#include "../adapters/arduino/arduino_board_pins.h"
#include "../adapters/arduino/arduino_presenter.h"
#include "../adapters/wokwi/wokwi_buttons.h"
#include "../use_cases/device_controller.h"

// >0 = type error messages one character at a time (Serial Monitor); 0 = print instantly.
#ifndef DEBUG_ERR_CHAR_MS
#define DEBUG_ERR_CHAR_MS 12
#endif

// Servo angles (degrees): full retract reference vs. retract-after-remove.
static const int ANGLE_HOME = 0;
static const int ANGLE_REMOVE = 30;
static const int ANGLE_INSERT = 152;
static const int MAX_DEPTH_MM = 50;
static const int DEFAULT_DEPTH_MM = 35;
static const int DEFAULT_SPEED_MM_S = 20;

static DeviceController g_dc;

void cardInserterApp_setup() {
  device_arduino_hw_init(9600, PIN_SERVO_PWM, ANGLE_HOME);

  device_wokwi_buttons_setup_pinmodes();

  DeviceConfig cfg;
  cfg.angle_home = ANGLE_HOME;
  cfg.angle_remove = ANGLE_REMOVE;
  cfg.angle_insert = ANGLE_INSERT;
  cfg.max_depth_mm = MAX_DEPTH_MM;
  cfg.default_depth_mm = DEFAULT_DEPTH_MM;
  cfg.default_speed_mm_s = DEFAULT_SPEED_MM_S;

  DevicePorts ports;
  device_arduino_presenter_bind_device_ports(&ports, DEBUG_ERR_CHAR_MS);

  g_dc.Init(cfg, ports);

  // If E-stop is already asserted at boot, enter ERROR immediately.
  g_dc.OnEstop();

  Serial.println(F("// Buttons ~ REST: INSERT HOME REMOVE ABORT RESET; STATUS EVENTS; RESERVE RELEASE; E-STOP"));
}

void cardInserterApp_loop() {
  g_dc.OnEstop();
  device_wokwi_buttons_poll(&g_dc, DEFAULT_DEPTH_MM, DEFAULT_SPEED_MM_S);
}

