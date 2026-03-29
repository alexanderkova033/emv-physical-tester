// Application layer: state machine + motion policy.

#pragma once

#include "../domain/device_types.h"
#include "../ports/device_ports.h"

typedef struct DeviceController {
  DeviceConfig cfg;
  DevicePorts ports;

  DeviceState state;
  ErrCode last_error;
  bool reserved;

  bool abort_motion;
  int current_angle;
  int last_commanded_angle;
  uint32_t motion_start_ms;
  uint32_t last_motion_duration_ms;

  DeviceState last_evt_old;
  DeviceState last_evt_new;
} DeviceController;

void device_init(DeviceController *dc, const DeviceConfig *cfg,
                 const DevicePorts *ports);

// **Use-cases / inbound boundary**
void device_api_home(DeviceController *dc);
void device_api_insert(DeviceController *dc, int depth_mm, int speed_mm_s);
void device_api_remove(DeviceController *dc);
void device_api_abort(DeviceController *dc);
void device_api_reset(DeviceController *dc);
void device_api_reserve(DeviceController *dc);
void device_api_release(DeviceController *dc);

DeviceStatus device_get_status(const DeviceController *dc);

// Hook for framework to notify the core about E-stop changes.
// Call this at the top of the main loop; it will move the device into ERROR
// (ERR_ESTOP) exactly once per assertion.
void device_on_estop(DeviceController *dc);

