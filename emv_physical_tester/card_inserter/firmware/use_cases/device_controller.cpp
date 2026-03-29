#include "device_controller.h"

static int clamp_i(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int depth_to_angle(const DeviceConfig *cfg, int depth_mm) {
  depth_mm = clamp_i(depth_mm, 0, cfg->max_depth_mm);
  long span = (long)cfg->angle_insert - cfg->angle_home;
  return cfg->angle_home + (int)(span * depth_mm / cfg->max_depth_mm);
}

static int speed_to_delay_ms(const DeviceConfig *cfg, int speed_mm_s) {
  (void)cfg;
  speed_mm_s = clamp_i(speed_mm_s, 5, 80);
  const int base = 12;
  return (int)(base * (long)20 / speed_mm_s);
}

static void emit_state(DeviceController *dc, DeviceState old_s,
                       DeviceState new_s) {
  dc->last_evt_old = old_s;
  dc->last_evt_new = new_s;
  if (dc->ports.emit_state_changed) {
    dc->ports.emit_state_changed(dc->ports.ctx, old_s, new_s);
  }
}

static void reject(DeviceController *dc, ErrCode e, const char *cmd,
                   const char *detail) {
  dc->last_error = e;
  if (dc->ports.log_err) {
    dc->ports.log_err(dc->ports.ctx, e, dc->state, cmd, detail);
  }
}

static void fault(DeviceController *dc, ErrCode e, const char *cmd,
                  const char *detail) {
  // Faults that should put the device into ERROR.
  if (dc->ports.log_err) {
    dc->ports.log_err(dc->ports.ctx, e, dc->state, cmd, detail);
  }
  dc->abort_motion = false;
  dc->last_error = e;
  DeviceState o = dc->state;
  dc->state = ST_ERROR;
  emit_state(dc, o, dc->state);
}

static void finish_user_abort(DeviceController *dc) {
  dc->abort_motion = false;
  dc->current_angle = dc->last_commanded_angle;
  DeviceState o = dc->state;
  dc->state = ST_IDLE;
  dc->last_error = ERR_NONE;
  dc->last_motion_duration_ms =
      (dc->motion_start_ms != 0) ? (dc->ports.now_ms(dc->ports.ctx) -
                                   dc->motion_start_ms)
                                 : 0;
  emit_state(dc, o, dc->state);
  if (dc->ports.log_ok) {
    dc->ports.log_ok(dc->ports.ctx,
                     "Abort complete — state IDLE (position held).");
  }
}

static void ramp_abortable(DeviceController *dc, int from_angle, int to_angle,
                           int steps, int delay_ms) {
  if (from_angle == to_angle || steps <= 0) return;
  float delta = (float)(to_angle - from_angle);
  for (int i = 0; i <= steps; i++) {
    if (dc->abort_motion ||
        (dc->ports.estop_asserted && dc->ports.estop_asserted(dc->ports.ctx))) {
      return;
    }
    int angle = from_angle + (int)(delta * i / (float)steps + 0.5f);
    dc->ports.servo_write_angle(dc->ports.ctx, angle);
    dc->last_commanded_angle = angle;
    dc->ports.delay_ms(dc->ports.ctx, (uint16_t)delay_ms);
  }
}

static void move_segmented_abortable(DeviceController *dc, int from_angle,
                                     int to_angle, int fast_steps,
                                     int fast_delay_ms, int slow_steps,
                                     int slow_delay_ms) {
  if (from_angle == to_angle) return;
  int mid = from_angle + (to_angle - from_angle) * 7 / 10;
  ramp_abortable(dc, from_angle, mid, fast_steps, fast_delay_ms);
  ramp_abortable(dc, mid, to_angle, slow_steps, slow_delay_ms);
}

void device_init(DeviceController *dc, const DeviceConfig *cfg,
                 const DevicePorts *ports) {
  dc->cfg = *cfg;
  dc->ports = *ports;

  dc->state = ST_IDLE;
  dc->last_error = ERR_NONE;
  dc->reserved = false;
  dc->abort_motion = false;

  dc->current_angle = cfg->angle_home;
  dc->last_commanded_angle = dc->current_angle;
  dc->motion_start_ms = 0;
  dc->last_motion_duration_ms = 0;

  dc->last_evt_old = ST_BOOTING;
  dc->last_evt_new = ST_BOOTING;
}

void device_api_home(DeviceController *dc) {
  const char *cmd = "POST /api/home";

  if (dc->state == ST_INSERTING || dc->state == ST_REMOVING ||
      dc->state == ST_HOMING) {
    reject(dc, ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }
  if (dc->state != ST_IDLE && dc->state != ST_INSERTED) {
    reject(dc, ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }

  dc->abort_motion = false;
  DeviceState o = dc->state;
  const bool from_inserted = (dc->state == ST_INSERTED);
  dc->state = ST_HOMING;
  dc->motion_start_ms = dc->ports.now_ms(dc->ports.ctx);
  emit_state(dc, o, dc->state);

  if (from_inserted) {
    ramp_abortable(dc, dc->current_angle, dc->cfg.angle_home, 55, 12);
  } else {
    move_segmented_abortable(dc, dc->current_angle, dc->cfg.angle_home, 55, 8,
                             45, 22);
  }

  if (dc->ports.estop_asserted && dc->ports.estop_asserted(dc->ports.ctx)) {
    // Preserve current position; move into error.
    dc->current_angle = dc->last_commanded_angle;
    fault(dc, ERR_ESTOP, "E-stop", nullptr);
    return;
  }
  if (dc->abort_motion) {
    finish_user_abort(dc);
    return;
  }

  dc->current_angle = dc->cfg.angle_home;
  dc->last_motion_duration_ms =
      dc->ports.now_ms(dc->ports.ctx) - dc->motion_start_ms;
  o = dc->state;
  dc->state = ST_IDLE;
  dc->last_error = ERR_NONE;
  emit_state(dc, o, dc->state);
  if (dc->ports.log_ok) {
    dc->ports.log_ok(dc->ports.ctx, "Home complete — state IDLE.");
  }
}

void device_api_insert(DeviceController *dc, int depth_mm, int speed_mm_s) {
  const char *cmd = "POST /api/insert";
  if (dc->state != ST_IDLE) {
    reject(dc, ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }

  const int target = depth_to_angle(&dc->cfg, depth_mm);
  const int dfast = speed_to_delay_ms(&dc->cfg, speed_mm_s);
  int dslow = dfast + dfast / 2;
  if (dslow > 35) dslow = 35;

  dc->abort_motion = false;
  DeviceState o = dc->state;
  dc->state = ST_INSERTING;
  dc->motion_start_ms = dc->ports.now_ms(dc->ports.ctx);
  emit_state(dc, o, dc->state);

  move_segmented_abortable(dc, dc->current_angle, target, 40, dfast, 45, dslow);

  if (dc->ports.estop_asserted && dc->ports.estop_asserted(dc->ports.ctx)) {
    dc->current_angle = dc->last_commanded_angle;
    fault(dc, ERR_ESTOP, "E-stop", nullptr);
    return;
  }
  if (dc->abort_motion) {
    finish_user_abort(dc);
    return;
  }

  dc->current_angle = target;
  dc->last_motion_duration_ms =
      dc->ports.now_ms(dc->ports.ctx) - dc->motion_start_ms;
  o = dc->state;
  dc->state = ST_INSERTED;
  dc->last_error = ERR_NONE;
  emit_state(dc, o, dc->state);
  if (dc->ports.log_ok) {
    dc->ports.log_ok(dc->ports.ctx, "Insert complete — state INSERTED.");
  }
}

void device_api_remove(DeviceController *dc) {
  const char *cmd = "POST /api/remove";
  if (dc->state != ST_INSERTED) {
    reject(dc, ERR_ILLEGAL_STATE, cmd, nullptr);
    return;
  }

  dc->abort_motion = false;
  DeviceState o = dc->state;
  dc->state = ST_REMOVING;
  dc->motion_start_ms = dc->ports.now_ms(dc->ports.ctx);
  emit_state(dc, o, dc->state);

  ramp_abortable(dc, dc->current_angle, dc->cfg.angle_remove, 55, 12);

  if (dc->ports.estop_asserted && dc->ports.estop_asserted(dc->ports.ctx)) {
    dc->current_angle = dc->last_commanded_angle;
    fault(dc, ERR_ESTOP, "E-stop", nullptr);
    return;
  }
  if (dc->abort_motion) {
    finish_user_abort(dc);
    return;
  }

  dc->current_angle = dc->cfg.angle_remove;
  dc->last_motion_duration_ms =
      dc->ports.now_ms(dc->ports.ctx) - dc->motion_start_ms;
  o = dc->state;
  dc->state = ST_IDLE;
  dc->last_error = ERR_NONE;
  emit_state(dc, o, dc->state);
  if (dc->ports.log_ok) {
    dc->ports.log_ok(dc->ports.ctx, "Remove complete — state IDLE.");
  }
}

void device_api_abort(DeviceController *dc) {
  if (dc->state != ST_INSERTING && dc->state != ST_REMOVING &&
      dc->state != ST_HOMING) {
    // Presentation handles the "ignored" message.
    return;
  }
  dc->abort_motion = true;
}

void device_api_reset(DeviceController *dc) {
  const char *cmd = "POST /api/reset";
  if (dc->ports.estop_asserted && dc->ports.estop_asserted(dc->ports.ctx)) {
    // Reset cannot complete during E-stop; remain in ERROR.
    if (dc->ports.log_err) {
      dc->ports.log_err(dc->ports.ctx, ERR_ESTOP, dc->state, cmd,
                        "Reset cannot finish while E-stop is still pressed. Release E-stop first.");
    }
    DeviceState o = dc->state;
    dc->last_error = ERR_ESTOP;
    dc->state = ST_ERROR;
    emit_state(dc, o, dc->state);
    return;
  }

  dc->abort_motion = false;
  dc->current_angle = dc->last_commanded_angle;
  DeviceState o = dc->state;
  dc->state = ST_IDLE;
  dc->last_error = ERR_NONE;
  emit_state(dc, o, dc->state);
  dc->ports.servo_write_angle(dc->ports.ctx, dc->current_angle);
  dc->last_commanded_angle = dc->current_angle;
  if (dc->ports.log_ok) {
    dc->ports.log_ok(dc->ports.ctx, "Reset complete — state IDLE.");
  }
}

void device_api_reserve(DeviceController *dc) {
  dc->reserved = true;
  if (dc->ports.emit_reservation) {
    dc->ports.emit_reservation(dc->ports.ctx, true);
  }
}

void device_api_release(DeviceController *dc) {
  dc->reserved = false;
  if (dc->ports.emit_reservation) {
    dc->ports.emit_reservation(dc->ports.ctx, false);
  }
}

DeviceStatus device_get_status(const DeviceController *dc) {
  DeviceStatus st;
  st.state = dc->state;
  st.last_error = dc->last_error;
  st.reserved = dc->reserved;
  st.motion_time_ms = dc->last_motion_duration_ms;
  st.last_evt_old = dc->last_evt_old;
  st.last_evt_new = dc->last_evt_new;
  return st;
}

void device_on_estop(DeviceController *dc) {
  if (!dc->ports.estop_asserted) return;
  if (!dc->ports.estop_asserted(dc->ports.ctx)) return;

  if (dc->state == ST_ERROR && dc->last_error == ERR_ESTOP) return;

  dc->abort_motion = true;
  dc->current_angle = dc->last_commanded_angle;

  if (dc->ports.log_err) {
    dc->ports.log_err(dc->ports.ctx, ERR_ESTOP, dc->state, "E-stop", nullptr);
  }

  DeviceState o = dc->state;
  dc->last_error = ERR_ESTOP;
  dc->state = ST_ERROR;
  emit_state(dc, o, dc->state);
}

