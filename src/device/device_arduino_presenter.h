#pragma once

#include <Arduino.h>
#include "device_core.h"

// Arduino/Serial presenter for the core's outbound port callbacks.
// Keeps PROGMEM + typing-effect behavior out of the core.

const char *device_state_name(DeviceState s);
const char *device_err_name(ErrCode e);

void device_serial_log_cmd(const char *line);
void device_serial_log_ok(const char *line);
void device_serial_log_err_typed(ErrCode e, DeviceState current_state,
                                 const char *command_label,
                                 const char *detail_override,
                                 uint16_t per_char_ms);

void device_serial_emit_state_changed(DeviceState old_s, DeviceState new_s);
void device_serial_emit_reservation(bool acquired);

void device_serial_print_status(const DeviceStatus *st);
void device_serial_print_last_event(DeviceState old_s, DeviceState new_s);

