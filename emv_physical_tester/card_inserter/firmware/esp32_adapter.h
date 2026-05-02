#pragma once

#include <stdint.h>
#include <WebServer.h>
#include "card_inserter_ports.h"
#include "card_inserter_domain_types.h"
#include "card_inserter_use_case_controller.h"

void esp32_hw_init(uint32_t serial_baud, int servo_pin, int initial_angle);

// Binds DevicePorts for the ESP32 target.
// server_ptr is stored in ctx and called inside port_delay_ms so that HTTP
// requests (e.g. abort) remain responsive during motion.
void esp32_bind_device_ports(DevicePorts* out, WebServer* server_ptr,
                             DeviceController* dc);

const char* device_state_name(DeviceState s);
const char* device_err_name(ErrCode e);
void device_serial_log_cmd(const char* line);
void device_serial_print_status(const DeviceStatus* st);
