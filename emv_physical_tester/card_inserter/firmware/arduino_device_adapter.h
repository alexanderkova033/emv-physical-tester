#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "card_inserter_ports.h"
#include "card_inserter_domain_types.h"

// Arduino adapter: Serial/protocol presentation and physical I/O that backs DevicePorts
// (servo PWM, estop GPIO, delay/millis). Keeps PROGMEM + typing-effect behavior out of the core.

void device_arduino_hw_init(uint32_t serial_baud, int servo_pwm_pin,
                            int initial_angle_deg);
void device_arduino_presenter_bind_device_ports(DevicePorts *out,
                                                uint16_t err_msg_char_ms);

const char* device_state_name(DeviceState s);
const char* device_err_name(ErrCode e);

void device_serial_log_cmd(const char* line);
void device_serial_log_ok(const char* line);
void device_serial_log_err_typed(ErrCode e, DeviceState current_state,
                                 const char* command_label,
                                 const char* detail_override,
                                 uint16_t per_char_ms);

void device_serial_emit_state_changed(DeviceState old_s, DeviceState new_s);
void device_serial_emit_reservation(bool acquired);

void device_serial_print_status(const DeviceStatus *st);
void device_serial_print_last_event(DeviceState old_s, DeviceState new_s);

