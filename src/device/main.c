#include <Servo.h>
#include <avr/pgmspace.h>

// >0 = type error messages one character at a time (Serial Monitor); 0 = print instantly.
#ifndef DEBUG_ERR_CHAR_MS
#define DEBUG_ERR_CHAR_MS 12
#endif

// Wokwi emulator: physical mapping for protocol-and-api-spec.md
//   POST /api/insert     -> D2 (blue)
//   POST /api/home       -> D3 (green)
//   POST /api/remove     -> D4 (yellow)
//   GET  /api/status     -> D5 (white) — prints JSON on Serial
//   POST /api/abort      -> D6 (red)
//   POST /api/reset      -> D7 (green)
//   GET  /api/events *   -> D8 (black) — prints last STATE_CHANGED line on Serial
//   POST /api/reserve *  -> D9 (blue)
//   POST /api/release *  -> D11 (yellow)
//   E-stop input         -> D12 (red); LOW = asserted (503 / ESTOP_ASSERTED)
// * Optional reservation endpoints per spec; SERIAL output mimics SSE "data:" lines.
// Servo PWM: D10

Servo carriage;

const int PIN_INSERT = 2;
const int PIN_HOME = 3;
const int PIN_REMOVE = 4;
const int PIN_STATUS = 5;
const int PIN_ABORT = 6;
const int PIN_RESET = 7;
const int PIN_EVENTS = 8;
const int PIN_RESERVE = 9;
const int PIN_RELEASE = 11;
const int PIN_ESTOP = 12;
const int PIN_SERVO_PWM = 10;

// Servo angles (degrees): full retract reference vs. retract-after-remove.
const int ANGLE_HOME = 0;
const int ANGLE_REMOVE = 30;
const int ANGLE_INSERT = 152;
const int MAX_DEPTH_MM = 50;
const int DEFAULT_DEPTH_MM = 35;
const int DEFAULT_SPEED_MM_S = 20;

enum DeviceState {
 ST_BOOTING,
 ST_HOMING,
 ST_IDLE,
 ST_INSERTING,
 ST_INSERTED,
 ST_REMOVING,
 ST_ERROR
};

enum ErrCode {
 ERR_NONE,
 ERR_ILLEGAL_STATE,
 ERR_HOME_FAILED,
 ERR_ESTOP
};

static DeviceState state = ST_BOOTING;
static ErrCode lastError = ERR_NONE;
static bool reserved = false;
static volatile bool g_abortMotion = false;
static int currentAngle = ANGLE_HOME;
static int lastCommandedAngle = ANGLE_HOME;
static unsigned long motionStartMs = 0;
static unsigned long lastMotionDurationMs = 0;

static DeviceState lastEvtOld = ST_BOOTING;
static DeviceState lastEvtNew = ST_BOOTING;

static int prevInsert = HIGH;
static int prevHome = HIGH;
static int prevRemove = HIGH;
static int prevStatus = HIGH;
static int prevAbort = HIGH;
static int prevReset = HIGH;
static int prevEvents = HIGH;
static int prevReserve = HIGH;
static int prevRelease = HIGH;

static void rampAbortable(int fromAngle, int toAngle, int steps, int delayMs);
static void moveSegmentedAbortable(int fromAngle, int toAngle, int fastSteps,
                                  int fastDelayMs, int slowSteps, int slowDelayMs);
static void emitStateChanged(DeviceState oldS, DeviceState newS);
static void serialPrintStatus(void);
static void serialPrintLastEvent(void);
static bool estopAsserted(void);
static void armEstopError(void);
static void handleMotionFault(ErrCode e, const __FlashStringHelper *commandLabel,
                            const __FlashStringHelper *detailOverride);
static void rejectCommand(ErrCode e, const __FlashStringHelper *commandLabel,
                         const __FlashStringHelper *detailOverride);
static void finishUserAbort(void);
void apiHome(void);
void apiInsert(int depthMm, int speedMmS);
void apiRemove(void);
void apiAbort(void);
void apiReset(void);

static const char *stateName(DeviceState s) {
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

static const char *errName(ErrCode e) {
 switch (e) {
 case ERR_NONE: return "NONE";
 case ERR_ILLEGAL_STATE: return "ILLEGAL_STATE";
 case ERR_HOME_FAILED: return "HOME_FAILED";
 case ERR_ESTOP: return "ESTOP_ASSERTED";
 default: return "INTERNAL_ERROR";
 }
}

static void typeFlash(const __FlashStringHelper *msg, uint16_t perCharMs) {
 PGM_P p = reinterpret_cast<PGM_P>(msg);
 for (;;) {
  uint8_t c = pgm_read_byte(p++);
  if (c == 0) {
   break;
  }
  Serial.write(c);
  if (perCharMs != 0) {
   delay(perCharMs);
  }
 }
}

static void logCmd(const __FlashStringHelper *line) {
 Serial.print(F("[CMD] "));
 Serial.println(line);
}

static void logOk(const __FlashStringHelper *line) {
 Serial.print(F("[OK]  "));
 Serial.println(line);
}

static void logErrTyped(ErrCode e, const __FlashStringHelper *commandLabel,
                       const __FlashStringHelper *detailOverride) {
 Serial.println(F("[ERR]"));
 Serial.print(F("  error_code: "));
 Serial.println(errName(e));
 Serial.print(F("  command: "));
 if (commandLabel) {
  Serial.println(commandLabel);
 } else {
  Serial.println(F("(unknown)"));
 }
 Serial.print(F("  state: "));
 Serial.println(stateName(state));
 Serial.print(F("  details: "));
 if (detailOverride) {
  typeFlash(detailOverride, DEBUG_ERR_CHAR_MS);
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
  typeFlash(story, DEBUG_ERR_CHAR_MS);
 }
 Serial.println();
 Serial.println(F("[ERR] end"));
}

static int depthToAngle(int depthMm) {
 if (depthMm < 0) depthMm = 0;
 if (depthMm > MAX_DEPTH_MM) depthMm = MAX_DEPTH_MM;
 long span = (long)ANGLE_INSERT - ANGLE_HOME;
 return ANGLE_HOME + (int)(span * depthMm / MAX_DEPTH_MM);
}

static int speedToDelayMs(int speedMmS) {
 if (speedMmS < 5) speedMmS = 5;
 if (speedMmS > 80) speedMmS = 80;
 int base = 12;
 return (int)(base * (long)DEFAULT_SPEED_MM_S / speedMmS);
}

void setup() {
 Serial.begin(9600);

 carriage.attach(PIN_SERVO_PWM);
 carriage.write(currentAngle);
 lastCommandedAngle = currentAngle;

 pinMode(PIN_INSERT, INPUT_PULLUP);
 pinMode(PIN_HOME, INPUT_PULLUP);
 pinMode(PIN_REMOVE, INPUT_PULLUP);
 pinMode(PIN_STATUS, INPUT_PULLUP);
 pinMode(PIN_ABORT, INPUT_PULLUP);
 pinMode(PIN_RESET, INPUT_PULLUP);
 pinMode(PIN_EVENTS, INPUT_PULLUP);
 pinMode(PIN_RESERVE, INPUT_PULLUP);
 pinMode(PIN_RELEASE, INPUT_PULLUP);
 pinMode(PIN_ESTOP, INPUT_PULLUP);

 state = ST_IDLE;
 lastError = ERR_NONE;
 g_abortMotion = false;

 if (estopAsserted()) {
  armEstopError();
 }

 Serial.println(F("// Buttons ~ REST: INSERT HOME REMOVE ABORT RESET; STATUS EVENTS; RESERVE RELEASE; E-STOP"));
}

void loop() {
 if (estopAsserted()) {
  if (state != ST_ERROR || lastError != ERR_ESTOP) {
   g_abortMotion = true;
   armEstopError();
  }
 }

 int ins = digitalRead(PIN_INSERT);
 int hom = digitalRead(PIN_HOME);
 int rem = digitalRead(PIN_REMOVE);
 int st = digitalRead(PIN_STATUS);
 int ab = digitalRead(PIN_ABORT);
 int rst = digitalRead(PIN_RESET);
 int ev = digitalRead(PIN_EVENTS);
 int res = digitalRead(PIN_RESERVE);
 int rel = digitalRead(PIN_RELEASE);

 if (st == LOW && prevStatus == HIGH) {
  logCmd(F("GET /api/status"));
  serialPrintStatus();
 }
 if (ev == LOW && prevEvents == HIGH) {
  logCmd(F("GET /api/events (last STATE_CHANGED)"));
  serialPrintLastEvent();
 }
 if (res == LOW && prevReserve == HIGH) {
  logCmd(F("POST /api/reserve"));
  reserved = true;
  Serial.println(F("data: {\"type\":\"RESERVATION\",\"owner\":\"wokwi\",\"action\":\"ACQUIRED\"}"));
 }
 if (rel == LOW && prevRelease == HIGH) {
  logCmd(F("POST /api/release"));
  reserved = false;
  Serial.println(F("data: {\"type\":\"RESERVATION\",\"owner\":\"wokwi\",\"action\":\"RELEASED\"}"));
 }

 if (ab == LOW && prevAbort == HIGH) {
  logCmd(F("POST /api/abort"));
  apiAbort();
 }
 if (rst == LOW && prevReset == HIGH) {
  logCmd(F("POST /api/reset"));
  apiReset();
 }

 if (!estopAsserted()) {
  if (ins == LOW && prevInsert == HIGH) {
   logCmd(F("POST /api/insert"));
   apiInsert(DEFAULT_DEPTH_MM, DEFAULT_SPEED_MM_S);
  }
  if (hom == LOW && prevHome == HIGH) {
   logCmd(F("POST /api/home"));
   apiHome();
  }
  if (rem == LOW && prevRemove == HIGH) {
   logCmd(F("POST /api/remove"));
   apiRemove();
  }
 }

 prevInsert = ins;
 prevHome = hom;
 prevRemove = rem;
 prevStatus = st;
 prevAbort = ab;
 prevReset = rst;
 prevEvents = ev;
 prevReserve = res;
 prevRelease = rel;
}

static bool estopAsserted(void) {
 return digitalRead(PIN_ESTOP) == LOW;
}

static void armEstopError(void) {
 currentAngle = lastCommandedAngle;
 logErrTyped(ERR_ESTOP, F("E-stop (D12)"), NULL);
 DeviceState o = state;
 lastError = ERR_ESTOP;
 state = ST_ERROR;
 emitStateChanged(o, state);
}

static void handleMotionFault(ErrCode e, const __FlashStringHelper *commandLabel,
                            const __FlashStringHelper *detailOverride) {
 // Faults that should place the device in ERROR.
 logErrTyped(e, commandLabel, detailOverride);
 g_abortMotion = false;
 lastError = e;
 DeviceState o = state;
 state = ST_ERROR;
 emitStateChanged(o, state);
}

static void rejectCommand(ErrCode e, const __FlashStringHelper *commandLabel,
                         const __FlashStringHelper *detailOverride) {
 // Command rejected without changing state (e.g. ILLEGAL_STATE / 409 in spec).
 logErrTyped(e, commandLabel, detailOverride);
 lastError = e;
 // No state transition here by design.
}

static void finishUserAbort(void) {
 g_abortMotion = false;
 currentAngle = lastCommandedAngle;
 DeviceState o = state;
 state = ST_IDLE;
 lastError = ERR_NONE;
 lastMotionDurationMs = (motionStartMs != 0) ? (millis() - motionStartMs) : 0;
 emitStateChanged(o, state);
 logOk(F("Abort complete — state IDLE (position held)."));
}

void apiHome(void) {
 if (state == ST_INSERTING || state == ST_REMOVING || state == ST_HOMING) {
  rejectCommand(ERR_ILLEGAL_STATE, F("POST /api/home"), NULL);
  return;
 }
 if (state != ST_IDLE && state != ST_INSERTED) {
  rejectCommand(ERR_ILLEGAL_STATE, F("POST /api/home"), NULL);
  return;
 }

 g_abortMotion = false;
 DeviceState o = state;
 bool fromInserted = (state == ST_INSERTED);
 state = ST_HOMING;
 motionStartMs = millis();
 emitStateChanged(o, state);

 if (fromInserted) {
  rampAbortable(currentAngle, ANGLE_HOME, 55, 12);
 } else {
  moveSegmentedAbortable(currentAngle, ANGLE_HOME, 55, 8, 45, 22);
 }

 if (estopAsserted()) {
  armEstopError();
  return;
 }
 if (g_abortMotion) {
  finishUserAbort();
  return;
 }

 currentAngle = ANGLE_HOME;
 lastMotionDurationMs = millis() - motionStartMs;
 o = state;
 state = ST_IDLE;
 lastError = ERR_NONE;
 emitStateChanged(o, state);
 logOk(F("Home complete — state IDLE."));
}

void apiInsert(int depthMm, int speedMmS) {
 if (state != ST_IDLE) {
  rejectCommand(ERR_ILLEGAL_STATE, F("POST /api/insert"), NULL);
  return;
 }

 int target = depthToAngle(depthMm);
 int dfast = speedToDelayMs(speedMmS);
 int dslow = dfast + dfast / 2;
 if (dslow > 35) dslow = 35;

 g_abortMotion = false;
 DeviceState o = state;
 state = ST_INSERTING;
 motionStartMs = millis();
 emitStateChanged(o, state);

 moveSegmentedAbortable(currentAngle, target, 40, dfast, 45, dslow);

 if (estopAsserted()) {
  armEstopError();
  return;
 }
 if (g_abortMotion) {
  finishUserAbort();
  return;
 }

 currentAngle = target;
 lastMotionDurationMs = millis() - motionStartMs;
 o = state;
 state = ST_INSERTED;
 lastError = ERR_NONE;
 emitStateChanged(o, state);
 logOk(F("Insert complete — state INSERTED."));
}

void apiRemove(void) {
 if (state != ST_INSERTED) {
  rejectCommand(ERR_ILLEGAL_STATE, F("POST /api/remove"), NULL);
  return;
 }

 g_abortMotion = false;
 DeviceState o = state;
 state = ST_REMOVING;
 motionStartMs = millis();
 emitStateChanged(o, state);

 rampAbortable(currentAngle, ANGLE_REMOVE, 55, 12);

 if (estopAsserted()) {
  armEstopError();
  return;
 }
 if (g_abortMotion) {
  finishUserAbort();
  return;
 }

 currentAngle = ANGLE_REMOVE;
 lastMotionDurationMs = millis() - motionStartMs;
 o = state;
 state = ST_IDLE;
 lastError = ERR_NONE;
 emitStateChanged(o, state);
 logOk(F("Remove complete — state IDLE."));
}

void apiAbort(void) {
 if (state != ST_INSERTING && state != ST_REMOVING && state != ST_HOMING) {
  Serial.println(F("[CMD]        (ignored — not in HOMING / INSERTING / REMOVING)"));
  return;
 }
 g_abortMotion = true;
 Serial.println(F("[CMD]        motion stop requested"));
}

void apiReset(void) {
 if (estopAsserted()) {
  logErrTyped(ERR_ESTOP, F("POST /api/reset"),
             F("Reset cannot finish while E-stop is still pressed. Release E-stop first."));
  DeviceState o = state;
  lastError = ERR_ESTOP;
  state = ST_ERROR;
  emitStateChanged(o, state);
  return;
 }

 g_abortMotion = false;
 currentAngle = lastCommandedAngle;
 DeviceState o = state;
 state = ST_IDLE;
 lastError = ERR_NONE;
 emitStateChanged(o, state);
 carriage.write(currentAngle);
 lastCommandedAngle = currentAngle;
 logOk(F("Reset complete — state IDLE."));
}

static void emitStateChanged(DeviceState oldS, DeviceState newS) {
 lastEvtOld = oldS;
 lastEvtNew = newS;
 Serial.print(F("data: {\"type\":\"STATE_CHANGED\",\"old_state\":\""));
 Serial.print(stateName(oldS));
 Serial.print(F("\",\"new_state\":\""));
 Serial.print(stateName(newS));
 Serial.println(F("\"}"));
}

static void serialPrintStatus(void) {
 Serial.print(F("{\"status\":\"OK\",\"state\":\""));
 Serial.print(stateName(state));
 Serial.print(F("\",\"last_error_code\":\""));
 Serial.print(errName(lastError));
 Serial.print(F("\",\"last_error_message\":\"NONE\",\"protocol_version\":1,"));
 Serial.print(F("\"min_compatible_protocol_version\":1,\"features\":[\"EVENTS\",\"RESET\",\"RESERVATION\"],"));
 Serial.print(F("\"reserved\":"));
 Serial.print(reserved ? F("true") : F("false"));
 Serial.print(F(",\"motion_time_ms\":"));
 Serial.print(lastMotionDurationMs);
 Serial.println(F("}"));
}

static void serialPrintLastEvent(void) {
 Serial.print(F("data: {\"type\":\"STATE_CHANGED\",\"old_state\":\""));
 Serial.print(stateName(lastEvtOld));
 Serial.print(F("\",\"new_state\":\""));
 Serial.print(stateName(lastEvtNew));
 Serial.println(F("\"}"));
}

static void rampAbortable(int fromAngle, int toAngle, int steps, int delayMs) {
 if (fromAngle == toAngle || steps <= 0) {
  return;
 }
 float delta = toAngle - fromAngle;
 for (int i = 0; i <= steps; i++) {
  if (g_abortMotion || estopAsserted()) {
   return;
  }
  int angle = fromAngle + (int)(delta * i / (float)steps + 0.5f);
  carriage.write(angle);
  lastCommandedAngle = angle;
  delay(delayMs);
 }
}

static void moveSegmentedAbortable(int fromAngle, int toAngle, int fastSteps,
                                  int fastDelayMs, int slowSteps, int slowDelayMs) {
 if (fromAngle == toAngle) {
  return;
 }
 int mid = fromAngle + (toAngle - fromAngle) * 7 / 10;
 rampAbortable(fromAngle, mid, fastSteps, fastDelayMs);
 rampAbortable(mid, toAngle, slowSteps, slowDelayMs);
}
