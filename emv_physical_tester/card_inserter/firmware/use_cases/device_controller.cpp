#include "device_controller.h"

#include <algorithm>

int DeviceController::DepthToAngle(const DeviceConfig& cfg, int depth_mm) {
  depth_mm = std::clamp(depth_mm, 0, cfg.max_depth_mm);
  const long span = static_cast<long>(cfg.angle_insert) - cfg.angle_home;
  return cfg.angle_home +
         static_cast<int>(span * depth_mm / cfg.max_depth_mm);
}

int DeviceController::SpeedToDelayMs(const DeviceConfig& cfg, int speed_mm_s) {
  (void)cfg;
  speed_mm_s = std::clamp(speed_mm_s, 5, 80);
  constexpr int kBase = 12;
  return static_cast<int>(kBase * static_cast<long>(20) / speed_mm_s);
}

void DeviceController::EmitState(DeviceState old_s, DeviceState new_s) {
  last_evt_old_ = old_s;
  last_evt_new_ = new_s;
  if (ports_.emit_state_changed) {
    ports_.emit_state_changed(ports_.ctx, old_s, new_s);
  }
}

void DeviceController::Reject(ErrCode e, const char* cmd, const char* detail) {
  last_error_ = e;
  if (ports_.log_err) {
    ports_.log_err(ports_.ctx, e, state_, cmd, detail);
  }
}

void DeviceController::Fault(ErrCode e, const char* cmd, const char* detail) {
  // Faults that should put the device into ERROR.
  if (ports_.log_err) {
    ports_.log_err(ports_.ctx, e, state_, cmd, detail);
  }
  abort_motion_ = false;
  last_error_ = e;
  const DeviceState old_state = state_;
  state_ = ST_ERROR;
  EmitState(old_state, state_);
}

void DeviceController::FinishUserAbort() {
  abort_motion_ = false;
  current_angle_ = last_commanded_angle_;
  const DeviceState old_state = state_;
  state_ = ST_IDLE;
  last_error_ = ERR_NONE;
  last_motion_duration_ms_ =
      (motion_start_ms_ != 0)
          ? (ports_.now_ms(ports_.ctx) - motion_start_ms_)
          : 0;
  EmitState(old_state, state_);
  if (ports_.log_ok) {
    ports_.log_ok(ports_.ctx, "Abort complete — state IDLE (position held).");
  }
}

void DeviceController::RampAbortable(int from_angle, int to_angle, int steps,
                                     int delay_ms) {
  if (from_angle == to_angle || steps <= 0) return;
  const float delta = static_cast<float>(to_angle - from_angle);
  for (int i = 0; i <= steps; i++) {
    if (abort_motion_ ||
        (ports_.estop_asserted && ports_.estop_asserted(ports_.ctx))) {
      return;
    }
    const int angle =
        from_angle +
        static_cast<int>(delta * i / static_cast<float>(steps) + 0.5f);
    ports_.servo_write_angle(ports_.ctx, angle);
    last_commanded_angle_ = angle;
    ports_.delay_ms(ports_.ctx, static_cast<std::uint16_t>(delay_ms));
  }
}

void DeviceController::MoveSegmentedAbortable(int from_angle, int to_angle,
                                              int fast_steps,
                                              int fast_delay_ms, int slow_steps,
                                              int slow_delay_ms) {
  if (from_angle == to_angle) return;
  const int mid = from_angle + (to_angle - from_angle) * 7 / 10;
  RampAbortable(from_angle, mid, fast_steps, fast_delay_ms);
  RampAbortable(mid, to_angle, slow_steps, slow_delay_ms);
}

void DeviceController::Init(const DeviceConfig& cfg, const DevicePorts& ports) {
  cfg_ = cfg;
  ports_ = ports;

  state_ = ST_IDLE;
  last_error_ = ERR_NONE;
  reserved_ = false;
  abort_motion_ = false;

  current_angle_ = cfg.angle_home;
  last_commanded_angle_ = current_angle_;
  motion_start_ms_ = 0;
  last_motion_duration_ms_ = 0;

  last_evt_old_ = ST_BOOTING;
  last_evt_new_ = ST_BOOTING;
}

void DeviceController::Home() {
  const char* cmd = "POST /api/home";

  if (state_ == ST_INSERTING || state_ == ST_REMOVING || state_ == ST_HOMING) {
    Reject(ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }
  if (state_ != ST_IDLE && state_ != ST_INSERTED) {
    Reject(ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }

  abort_motion_ = false;
  DeviceState old_state = state_;
  const bool from_inserted = (state_ == ST_INSERTED);
  state_ = ST_HOMING;
  motion_start_ms_ = ports_.now_ms(ports_.ctx);
  EmitState(old_state, state_);

  if (from_inserted) {
    RampAbortable(current_angle_, cfg_.angle_home, 55, 12);
  } else {
    MoveSegmentedAbortable(current_angle_, cfg_.angle_home, 55, 8, 45, 22);
  }

  if (ports_.estop_asserted && ports_.estop_asserted(ports_.ctx)) {
    // Preserve current position; move into error.
    current_angle_ = last_commanded_angle_;
    Fault(ERR_ESTOP, "E-stop", nullptr);
    return;
  }
  if (abort_motion_) {
    FinishUserAbort();
    return;
  }

  current_angle_ = cfg_.angle_home;
  last_motion_duration_ms_ = ports_.now_ms(ports_.ctx) - motion_start_ms_;
  old_state = state_;
  state_ = ST_IDLE;
  last_error_ = ERR_NONE;
  EmitState(old_state, state_);
  if (ports_.log_ok) {
    ports_.log_ok(ports_.ctx, "Home complete — state IDLE.");
  }
}

void DeviceController::Insert(int depth_mm, int speed_mm_s) {
  const char* cmd = "POST /api/insert";
  if (state_ != ST_IDLE) {
    Reject(ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }

  const int target = DepthToAngle(cfg_, depth_mm);
  const int dfast = SpeedToDelayMs(cfg_, speed_mm_s);
  int dslow = dfast + dfast / 2;
  if (dslow > 35) dslow = 35;

  abort_motion_ = false;
  DeviceState old_state = state_;
  state_ = ST_INSERTING;
  motion_start_ms_ = ports_.now_ms(ports_.ctx);
  EmitState(old_state, state_);

  MoveSegmentedAbortable(current_angle_, target, 40, dfast, 45, dslow);

  if (ports_.estop_asserted && ports_.estop_asserted(ports_.ctx)) {
    current_angle_ = last_commanded_angle_;
    Fault(ERR_ESTOP, "E-stop", nullptr);
    return;
  }
  if (abort_motion_) {
    FinishUserAbort();
    return;
  }

  current_angle_ = target;
  last_motion_duration_ms_ = ports_.now_ms(ports_.ctx) - motion_start_ms_;
  old_state = state_;
  state_ = ST_INSERTED;
  last_error_ = ERR_NONE;
  EmitState(old_state, state_);
  if (ports_.log_ok) {
    ports_.log_ok(ports_.ctx, "Insert complete — state INSERTED.");
  }
}

void DeviceController::Remove() {
  const char* cmd = "POST /api/remove";
  if (state_ != ST_INSERTED) {
    Reject(ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }

  abort_motion_ = false;
  DeviceState old_state = state_;
  state_ = ST_REMOVING;
  motion_start_ms_ = ports_.now_ms(ports_.ctx);
  EmitState(old_state, state_);

  RampAbortable(current_angle_, cfg_.angle_remove, 55, 12);

  if (ports_.estop_asserted && ports_.estop_asserted(ports_.ctx)) {
    current_angle_ = last_commanded_angle_;
    Fault(ERR_ESTOP, "E-stop", nullptr);
    return;
  }
  if (abort_motion_) {
    FinishUserAbort();
    return;
  }

  current_angle_ = cfg_.angle_remove;
  last_motion_duration_ms_ = ports_.now_ms(ports_.ctx) - motion_start_ms_;
  old_state = state_;
  state_ = ST_IDLE;
  last_error_ = ERR_NONE;
  EmitState(old_state, state_);
  if (ports_.log_ok) {
    ports_.log_ok(ports_.ctx, "Remove complete — state IDLE.");
  }
}

void DeviceController::Abort() {
  if (state_ != ST_INSERTING && state_ != ST_REMOVING && state_ != ST_HOMING) {
    // Presentation handles the "ignored" message.
    return;
  }
  abort_motion_ = true;
}

void DeviceController::Reset() {
  const char* cmd = "POST /api/reset";
  if (ports_.estop_asserted && ports_.estop_asserted(ports_.ctx)) {
    // Reset cannot complete during E-stop; remain in ERROR.
    if (ports_.log_err) {
      ports_.log_err(ports_.ctx, ERR_ESTOP, state_, cmd,
                     "Reset cannot finish while E-stop is still pressed. Release E-stop first.");
    }
    const DeviceState old_state = state_;
    last_error_ = ERR_ESTOP;
    state_ = ST_ERROR;
    EmitState(old_state, state_);
    return;
  }

  abort_motion_ = false;
  current_angle_ = last_commanded_angle_;
  const DeviceState old_state = state_;
  state_ = ST_IDLE;
  last_error_ = ERR_NONE;
  EmitState(old_state, state_);
  ports_.servo_write_angle(ports_.ctx, current_angle_);
  last_commanded_angle_ = current_angle_;
  if (ports_.log_ok) {
    ports_.log_ok(ports_.ctx, "Reset complete — state IDLE.");
  }
}

void DeviceController::Reserve() {
  reserved_ = true;
  if (ports_.emit_reservation) {
    ports_.emit_reservation(ports_.ctx, true);
  }
}

void DeviceController::Release() {
  reserved_ = false;
  if (ports_.emit_reservation) {
    ports_.emit_reservation(ports_.ctx, false);
  }
}

DeviceStatus DeviceController::GetStatus() const {
  DeviceStatus st{};
  st.state = state_;
  st.last_error = last_error_;
  st.reserved = reserved_;
  st.motion_time_ms = last_motion_duration_ms_;
  st.last_evt_old = last_evt_old_;
  st.last_evt_new = last_evt_new_;
  return st;
}

void DeviceController::OnEstop() {
  if (!ports_.estop_asserted) return;
  if (!ports_.estop_asserted(ports_.ctx)) return;

  if (state_ == ST_ERROR && last_error_ == ERR_ESTOP) return;

  abort_motion_ = true;
  current_angle_ = last_commanded_angle_;

  if (ports_.log_err) {
    ports_.log_err(ports_.ctx, ERR_ESTOP, state_, "E-stop", nullptr);
  }

  const DeviceState old_state = state_;
  last_error_ = ERR_ESTOP;
  state_ = ST_ERROR;
  EmitState(old_state, state_);
}

