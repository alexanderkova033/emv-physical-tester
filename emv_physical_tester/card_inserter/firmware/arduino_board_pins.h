#pragma once

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
// Servo PWM: D10

#define PIN_INSERT 2
#define PIN_HOME 3
#define PIN_REMOVE 4
#define PIN_STATUS 5
#define PIN_ABORT 6
#define PIN_RESET 7
#define PIN_EVENTS 8
#define PIN_RESERVE 9
#define PIN_RELEASE 11
#define PIN_ESTOP 12
#define PIN_SERVO_PWM 10

