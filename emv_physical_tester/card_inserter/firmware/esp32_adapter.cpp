#include "esp32_adapter.h"
#include "esp32_pins.h"

#include <Arduino.h>
#include <ESP32Servo.h>
#include <WebServer.h>

namespace {

Servo g_servo;

struct Ctx {
    WebServer* server;
};

Ctx g_ctx{};

// ── Port implementations ──────────────────────────────────────────────────────

bool port_estop_asserted(void* ctx) {
    (void)ctx;
    return digitalRead(PIN_ESTOP) == LOW;
}

void port_servo_write(void* ctx, int angle) {
    (void)ctx;
    g_servo.write(angle);
}

// Pump the HTTP server during each delay slice so abort/status arrive while
// the ramp loop is running.
void port_delay_ms(void* ctx, uint16_t ms) {
    Ctx* c = static_cast<Ctx*>(ctx);
    const uint32_t end = millis() + ms;
    while ((int32_t)(end - millis()) > 0) {
        if (c->server) c->server->handleClient();
        delay(1);
    }
}

uint32_t port_now_ms(void* ctx) {
    (void)ctx;
    return millis();
}

void port_emit_state_changed(void* ctx, DeviceState old_s, DeviceState new_s) {
    (void)ctx;
    Serial.print(F("data: {\"type\":\"STATE_CHANGED\",\"old_state\":\""));
    Serial.print(device_state_name(old_s));
    Serial.print(F("\",\"new_state\":\""));
    Serial.print(device_state_name(new_s));
    Serial.println(F("\"}"));
}

void port_emit_reservation(void* ctx, bool acquired) {
    (void)ctx;
    Serial.print(F("data: {\"type\":\"RESERVATION\",\"owner\":\"button_board\",\"action\":\""));
    Serial.print(acquired ? "ACQUIRED" : "RELEASED");
    Serial.println(F("\"}"));
}

void port_log_cmd(void* ctx, const char* line) {
    (void)ctx;
    Serial.print(F("[CMD] "));
    Serial.println(line);
}

void port_log_ok(void* ctx, const char* line) {
    (void)ctx;
    Serial.print(F("[OK]  "));
    Serial.println(line);
}

void port_log_err(void* ctx, ErrCode e, DeviceState s, const char* cmd,
                  const char* detail) {
    (void)ctx;
    Serial.print(F("[ERR] "));
    Serial.print(device_err_name(e));
    Serial.print(F(" state="));
    Serial.print(device_state_name(s));
    if (cmd) { Serial.print(F(" cmd=")); Serial.print(cmd); }
    if (detail) { Serial.print(F(" ")); Serial.print(detail); }
    Serial.println();
}

void port_log_trace(void* ctx, const char* line) {
    (void)ctx;
    if (!line) return;
    Serial.print(F("[RAMP] "));
    Serial.println(line);
}

}  // namespace

// ── Public API ────────────────────────────────────────────────────────────────

const char* device_state_name(DeviceState s) {
    switch (s) {
        case ST_BOOTING:   return "BOOTING";
        case ST_HOMING:    return "HOMING";
        case ST_IDLE:      return "IDLE";
        case ST_INSERTING: return "INSERTING";
        case ST_INSERTED:  return "INSERTED";
        case ST_REMOVING:  return "REMOVING";
        case ST_ERROR:     return "ERROR";
        default:           return "UNKNOWN";
    }
}

const char* device_err_name(ErrCode e) {
    switch (e) {
        case ERR_NONE:          return "NONE";
        case ERR_ILLEGAL_STATE: return "ILLEGAL_STATE";
        case ERR_HOME_FAILED:   return "HOME_FAILED";
        case ERR_ESTOP:         return "ESTOP_ASSERTED";
        default:                return "INTERNAL_ERROR";
    }
}

void device_serial_log_cmd(const char* line) {
    Serial.print(F("[CMD] "));
    Serial.println(line);
}

void device_serial_print_status(const DeviceStatus* st) {
    Serial.print(F("{\"status\":\"OK\",\"state\":\""));
    Serial.print(device_state_name(st->state));
    Serial.print(F("\",\"last_error_code\":\""));
    Serial.print(device_err_name(st->last_error));
    Serial.print(F("\",\"last_error_message\":\"NONE\",\"protocol_version\":1,"));
    Serial.print(F("\"min_compatible_protocol_version\":1,\"features\":[\"RESET\"],"));
    Serial.print(F("\"motion_time_ms\":"));
    Serial.print(st->motion_time_ms);
    Serial.println(F("}"));
}

void esp32_hw_init(uint32_t serial_baud, int servo_pin, int initial_angle) {
    Serial.begin(serial_baud);
    pinMode(PIN_ESTOP,  INPUT_PULLUP);
    pinMode(PIN_INSERT, INPUT_PULLUP);
    pinMode(PIN_HOME,   INPUT_PULLUP);
    pinMode(PIN_REMOVE, INPUT_PULLUP);
    pinMode(PIN_STATUS, INPUT_PULLUP);
    pinMode(PIN_ABORT,  INPUT_PULLUP);
    g_servo.attach(servo_pin);
    g_servo.write(initial_angle);
}

void esp32_bind_device_ports(DevicePorts* out, WebServer* server_ptr) {
    g_ctx.server = server_ptr;
    out->ctx                = &g_ctx;
    out->estop_asserted     = port_estop_asserted;
    out->servo_write_angle  = port_servo_write;
    out->delay_ms           = port_delay_ms;
    out->now_ms             = port_now_ms;
    out->emit_state_changed = port_emit_state_changed;
    out->emit_reservation   = port_emit_reservation;
    out->log_cmd            = port_log_cmd;
    out->log_ok             = port_log_ok;
    out->log_err            = port_log_err;
    out->log_trace          = port_log_trace;
}
