// Outbound boundary for the Card Inserter core.
// The use-cases call these ports to interact with hardware and I/O.

#pragma once

#include <cstdint>

#include "../domain/device_types.h"

struct DevicePorts {
  void* ctx;

  // **Hardware/OS**
  bool (*estop_asserted)(void* ctx);
  void (*servo_write_angle)(void* ctx, int angle);
  void (*delay_ms)(void* ctx, std::uint16_t ms);
  std::uint32_t (*now_ms)(void* ctx);

  // **Presentation/logging**
  void (*emit_state_changed)(void* ctx, DeviceState old_s, DeviceState new_s);
  void (*emit_reservation)(void* ctx, bool acquired);
  void (*log_cmd)(void* ctx, const char* line);
  void (*log_ok)(void* ctx, const char* line);
  void (*log_err)(void* ctx, ErrCode e, DeviceState current_state,
                  const char* command_label, const char* detail_override);
};

