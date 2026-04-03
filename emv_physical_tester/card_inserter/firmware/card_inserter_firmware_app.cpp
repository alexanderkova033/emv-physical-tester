#include "card_inserter_firmware_app.h"

#include "button_board_pins.h"
#include "arduino_device_adapter.h"
#include "card_inserter_use_case_controller.h"
#include "button_board_input_adapter.h"

// >0 = type error messages one character at a time (Serial Monitor); 0 = print instantly.
#ifndef DEBUG_ERR_CHAR_MS
#define DEBUG_ERR_CHAR_MS 12
#endif

namespace {

// Servo angles (degrees): full retract reference vs. retract-after-remove.
constexpr int kAngleHome = 0;
constexpr int kAngleRemove = 30;
constexpr int kAngleInsert = 152;

constexpr int kMaxDepthMm = 50;
constexpr int kDefaultDepthMm = 35;
constexpr int kDefaultSpeedMmS = 20;

DeviceController g_dc;

DeviceConfig MakeDeviceConfig() {
  DeviceConfig cfg{};
  cfg.angle_home = kAngleHome;
  cfg.angle_remove = kAngleRemove;
  cfg.angle_insert = kAngleInsert;
  cfg.max_depth_mm = kMaxDepthMm;
  cfg.default_depth_mm = kDefaultDepthMm;
  cfg.default_speed_mm_s = kDefaultSpeedMmS;
  return cfg;
}

}  // namespace

void cardInserterApp_setup() {
  device_arduino_hw_init(9600, PIN_SERVO_PWM, kAngleHome);

  device_button_board_setup_pinmodes();

  const DeviceConfig cfg = MakeDeviceConfig();

  DevicePorts ports{};
  device_arduino_presenter_bind_device_ports(&ports, DEBUG_ERR_CHAR_MS);

  g_dc.Init(cfg, ports);

  // If E-stop is already asserted at boot, enter ERROR immediately.
  g_dc.OnEstop();

  Serial.println(F("// Buttons ~ REST (MVP): INSERT HOME REMOVE ABORT; STATUS — see protocol spec for full GPIO map"));
}

void cardInserterApp_loop() {
  g_dc.OnEstop();
  device_button_board_poll(&g_dc, kDefaultDepthMm, kDefaultSpeedMmS);
}

