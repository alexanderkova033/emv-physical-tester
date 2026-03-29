// Application layer: state machine + motion policy.

#pragma once

#include "../domain/device_types.h"
#include "../ports/device_ports.h"

class DeviceController {
 public:
  DeviceController() = default;

  void Init(const DeviceConfig& cfg, const DevicePorts& ports);

  // Use-cases / inbound boundary.
  void Home();
  void Insert(int depth_mm, int speed_mm_s);
  void Remove();
  void Abort();
  void Reset();
  void Reserve();
  void Release();

  DeviceStatus GetStatus() const;

  // Hook for framework to notify the core about E-stop changes.
  // Call this at the top of the main loop; it will move the device into ERROR
  // (ERR_ESTOP) exactly once per assertion.
  void OnEstop();

 private:
  static int DepthToAngle(const DeviceConfig& cfg, int depth_mm);
  static int SpeedToDelayMs(const DeviceConfig& cfg, int speed_mm_s);

  void EmitState(DeviceState old_s, DeviceState new_s);
  void Reject(ErrCode e, const char* cmd, const char* detail);
  void Fault(ErrCode e, const char* cmd, const char* detail);
  void FinishUserAbort();
  void RampAbortable(int from_angle, int to_angle, int steps, int delay_ms);
  void MoveSegmentedAbortable(int from_angle, int to_angle, int fast_steps,
                              int fast_delay_ms, int slow_steps,
                              int slow_delay_ms);

  DeviceConfig cfg_{};
  DevicePorts ports_{};

  DeviceState state_ = ST_BOOTING;
  ErrCode last_error_ = ERR_NONE;
  bool reserved_ = false;

  bool abort_motion_ = false;
  int current_angle_ = 0;
  int last_commanded_angle_ = 0;
  uint32_t motion_start_ms_ = 0;
  uint32_t last_motion_duration_ms_ = 0;

  DeviceState last_evt_old_ = ST_BOOTING;
  DeviceState last_evt_new_ = ST_BOOTING;
};

