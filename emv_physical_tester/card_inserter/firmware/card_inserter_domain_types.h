// Domain types for the EMV Physical Tester - Card Inserter.
// Intentionally framework-agnostic (no Arduino/Serial/Servo types).

#pragma once

#include <stdint.h>

enum DeviceState {
  ST_BOOTING = 0,
  ST_HOMING,
  ST_IDLE,
  ST_INSERTING,
  ST_INSERTED,
  ST_REMOVING,
  ST_ERROR,
};

enum ErrCode {
  ERR_NONE = 0,
  ERR_ILLEGAL_STATE,
  ERR_HOME_FAILED,
  ERR_ESTOP,
};

struct DeviceConfig {
  int angle_home = 0;
  int angle_remove = 0;
  int angle_insert = 0;
  int max_depth_mm = 0;
  int default_depth_mm = 0;
  int default_speed_mm_s = 0;
};

struct DeviceStatus {
  DeviceState state = ST_BOOTING;
  ErrCode last_error = ERR_NONE;
  bool reserved = false;
  uint32_t motion_time_ms = 0;
  DeviceState last_evt_old = ST_BOOTING;
  DeviceState last_evt_new = ST_BOOTING;
};

