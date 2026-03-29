#include "arduino_presenter.h"

#include <Servo.h>
#include <avr/pgmspace.h>
#include <cstdint>

#include "arduino_board_pins.h"

static Servo s_servo;
static std::uint16_t s_err_msg_char_ms;

const char *device_state_name(DeviceState s) {
  switch (s) {
    case ST_BOOTING: return "BOOTING";
    case ST_HOMING: return "HOMING";
    case ST_IDLE: return "IDLE";
    case ST_INSERTING: return "INSERTING";
    case ST_INSERTED: return "INSERTED";
    case ST_REMOVING: return "REMOVING";
    case ST_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

const char *device_err_name(ErrCode e) {
  switch (e) {
    case ERR_NONE: return "NONE";
    case ERR_ILLEGAL_STATE: return "ILLEGAL_STATE";
    case ERR_HOME_FAILED: return "HOME_FAILED";
    case ERR_ESTOP: return "ESTOP_ASSERTED";
    default: return "INTERNAL_ERROR";
  }
}

static void type_flash(const __FlashStringHelper *msg,
                       std::uint16_t per_char_ms) {
  PGM_P p = reinterpret_cast<PGM_P>(msg);
  for (;;) {
    std::uint8_t c = pgm_read_byte(p++);
    if (c == 0) break;
    Serial.write(c);
    if (per_char_ms != 0) delay(per_char_ms);
  }
}

void device_serial_log_cmd(const char *line) {
  Serial.print(F("[CMD] "));
  Serial.println(line);
}

void device_serial_log_ok(const char *line) {
  Serial.print(F("[OK]  "));
  Serial.println(line);
}

void device_serial_log_err_typed(ErrCode e, DeviceState current_state,
                                 const char *command_label,
                                 const char *detail_override,
                                 std::uint16_t per_char_ms) {
  Serial.println(F("[ERR]"));
  Serial.print(F("  error_code: "));
  Serial.println(device_err_name(e));
  Serial.print(F("  command: "));
  Serial.println(command_label ? command_label : "(unknown)");
  Serial.print(F("  state: "));
  Serial.println(device_state_name(current_state));

  Serial.print(F("  details: "));
  if (detail_override) {
    Serial.print(detail_override);
  } else {
    const __FlashStringHelper *story = F("Unexpected error.");
    switch (e) {
      case ERR_ILLEGAL_STATE:
        story = F(
            "This command is not allowed in the current state. Use the Status button "
            "(GET /api/status), then follow the normal sequence: Home, Insert, "
            "Remove, or Reset / Abort as needed.");
        break;
      case ERR_HOME_FAILED:
        story = F("Homing did not finish as expected. Inspect the mechanism and use "
                  "Reset when it is safe.");
        break;
      case ERR_ESTOP:
        story = F(
            "Emergency stop is asserted. Release the E-stop switch, then press "
            "Reset if the device stays in ERROR.");
        break;
      default:
        break;
    }
    type_flash(story, per_char_ms);
  }
  Serial.println();
  Serial.println(F("[ERR] end"));
}

void device_serial_emit_state_changed(DeviceState old_s, DeviceState new_s) {
  Serial.print(F("data: {\"type\":\"STATE_CHANGED\",\"old_state\":\""));
  Serial.print(device_state_name(old_s));
  Serial.print(F("\",\"new_state\":\""));
  Serial.print(device_state_name(new_s));
  Serial.println(F("\"}"));
}

void device_serial_emit_reservation(bool acquired) {
  Serial.print(F("data: {\"type\":\"RESERVATION\",\"owner\":\"wokwi\",\"action\":\""));
  Serial.print(acquired ? F("ACQUIRED") : F("RELEASED"));
  Serial.println(F("\"}"));
}

void device_serial_print_status(const DeviceStatus *st) {
  Serial.print(F("{\"status\":\"OK\",\"state\":\""));
  Serial.print(device_state_name(st->state));
  Serial.print(F("\",\"last_error_code\":\""));
  Serial.print(device_err_name(st->last_error));
  Serial.print(F("\",\"last_error_message\":\"NONE\",\"protocol_version\":1,"));
  Serial.print(F("\"min_compatible_protocol_version\":1,\"features\":[\"EVENTS\",\"RESET\",\"RESERVATION\"],"));
  Serial.print(F("\"reserved\":"));
  Serial.print(st->reserved ? F("true") : F("false"));
  Serial.print(F(",\"motion_time_ms\":"));
  Serial.print(st->motion_time_ms);
  Serial.println(F("}"));
}

void device_serial_print_last_event(DeviceState old_s, DeviceState new_s) {
  Serial.print(F("data: {\"type\":\"STATE_CHANGED\",\"old_state\":\""));
  Serial.print(device_state_name(old_s));
  Serial.print(F("\",\"new_state\":\""));
  Serial.print(device_state_name(new_s));
  Serial.println(F("\"}"));
}

namespace {

bool port_estop_asserted(void *ctx) {
  (void)ctx;
  return digitalRead(PIN_ESTOP) == LOW;
}

void port_servo_write(void *ctx, int angle) {
  (void)ctx;
  s_servo.write(angle);
}

void port_delay_ms(void *ctx, std::uint16_t ms) {
  (void)ctx;
  delay(ms);
}

std::uint32_t port_now_ms(void *ctx) {
  (void)ctx;
  return static_cast<std::uint32_t>(millis());
}

void port_emit_state_changed(void *ctx, DeviceState old_s, DeviceState new_s) {
  (void)ctx;
  device_serial_emit_state_changed(old_s, new_s);
}

void port_emit_reservation(void *ctx, bool acquired) {
  (void)ctx;
  device_serial_emit_reservation(acquired);
}

void port_log_cmd(void *ctx, const char *line) {
  (void)ctx;
  device_serial_log_cmd(line);
}

void port_log_ok(void *ctx, const char *line) {
  (void)ctx;
  device_serial_log_ok(line);
}

void port_log_err(void *ctx, ErrCode e, DeviceState current_state,
                  const char *command_label, const char *detail_override) {
  (void)ctx;
  device_serial_log_err_typed(e, current_state, command_label, detail_override,
                              s_err_msg_char_ms);
}

}  // namespace

void device_arduino_hw_init(std::uint32_t serial_baud, int servo_pwm_pin,
                            int initial_angle_deg) {
  Serial.begin(serial_baud);
  s_servo.attach(servo_pwm_pin);
  s_servo.write(initial_angle_deg);
}

void device_arduino_presenter_bind_device_ports(DevicePorts *out,
                                                std::uint16_t err_msg_char_ms) {
  s_err_msg_char_ms = err_msg_char_ms;
  out->ctx = nullptr;
  out->estop_asserted = port_estop_asserted;
  out->servo_write_angle = port_servo_write;
  out->delay_ms = port_delay_ms;
  out->now_ms = port_now_ms;
  out->emit_state_changed = port_emit_state_changed;
  out->emit_reservation = port_emit_reservation;
  out->log_cmd = port_log_cmd;
  out->log_ok = port_log_ok;
  out->log_err = port_log_err;
}

