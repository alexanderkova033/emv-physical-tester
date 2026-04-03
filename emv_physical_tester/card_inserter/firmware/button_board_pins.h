#pragma once

// MVP button board + emulator (Wokwi): physical buttons only for core lab debug.
//   POST /api/insert -> D2 (blue)
//   POST /api/home   -> D3 (green)
//   POST /api/remove -> D4 (yellow)
//   GET  /api/status -> D5 (white) — prints JSON on Serial
//   POST /api/abort  -> D6 (red)
// Full design may add D7 reset, D8 events, D9/D11 reserve/release, D12 E-stop; see protocol spec.
// Servo PWM: D10

#define PIN_INSERT 2
#define PIN_HOME 3
#define PIN_REMOVE 4
#define PIN_STATUS 5
#define PIN_ABORT 6
#define PIN_ESTOP 12
#define PIN_SERVO_PWM 10
