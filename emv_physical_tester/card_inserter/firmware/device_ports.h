// Outbound boundary for the Card Inserter core.
// The use-cases call these ports to interact with hardware and I/O.

#pragma once

#include <cstdint>

#include "device_types.h"

struct DevicePorts {
  void* ctx = nullptr;

  // **Hardware/OS**
  bool (*estop_asserted)(void* ctx) = nullptr;
  void (*servo_write_angle)(void* ctx, int angle) = nullptr;
  void (*delay_ms)(void* ctx, std::uint16_t ms) = nullptr;
  std::uint32_t (*now_ms)(void* ctx) = nullptr;

  // **Presentation/logging**
  void (*emit_state_changed)(void* ctx, DeviceState old_s, DeviceState new_s) =
      nullptr;
  void (*emit_reservation)(void* ctx, bool acquired) = nullptr;
  void (*log_cmd)(void* ctx, const char* line) = nullptr;
  void (*log_ok)(void* ctx, const char* line) = nullptr;
  void (*log_err)(void* ctx, ErrCode e, DeviceState current_state,
                  const char* command_label, const char* detail_override) =
      nullptr;
};

