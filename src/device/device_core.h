// Clean-architecture core (entities + use-cases) for the EMV physical tester.
// This layer owns the state machine and motion policy. It depends only on the
// abstract ports declared below (no Arduino / Serial / Servo types here).

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum DeviceState {
  ST_BOOTING = 0,
  ST_HOMING,
  ST_IDLE,
  ST_INSERTING,
  ST_INSERTED,
  ST_REMOVING,
  ST_ERROR
} DeviceState;

typedef enum ErrCode {
  ERR_NONE = 0,
  ERR_ILLEGAL_STATE,
  ERR_HOME_FAILED,
  ERR_ESTOP
} ErrCode;

typedef struct DeviceConfig {
  int angle_home;
  int angle_remove;
  int angle_insert;
  int max_depth_mm;
  int default_depth_mm;
  int default_speed_mm_s;
} DeviceConfig;

typedef struct DeviceStatus {
  DeviceState state;
  ErrCode last_error;
  bool reserved;
  uint32_t motion_time_ms;
  DeviceState last_evt_old;
  DeviceState last_evt_new;
} DeviceStatus;

// Outbound boundary: everything "outside" the use-cases.
// The core calls these function pointers to interact with hardware and I/O.
typedef struct DevicePorts {
  void *ctx;

  // **Hardware/OS**
  bool (*estop_asserted)(void *ctx);
  void (*servo_write_angle)(void *ctx, int angle);
  void (*delay_ms)(void *ctx, uint16_t ms);
  uint32_t (*now_ms)(void *ctx);

  // **Presentation/logging**
  void (*emit_state_changed)(void *ctx, DeviceState old_s, DeviceState new_s);
  void (*emit_reservation)(void *ctx, bool acquired);
  void (*log_cmd)(void *ctx, const char *line);
  void (*log_ok)(void *ctx, const char *line);
  void (*log_err)(void *ctx, ErrCode e, DeviceState current_state,
                  const char *command_label, const char *detail_override);
} DevicePorts;

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

