// Domain types for the EMV Physical Tester - Card Inserter.
// Intentionally framework-agnostic (no Arduino/Serial/Servo types).

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

