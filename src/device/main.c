#include <Servo.h>

// Maps the single-axis card carriage to servo angle (Wokwi stand-in for lead screw / stepper).
// Behavior follows design/mechanical-and-electronics-concept.md (Firmware / Motion, section 4):
//   - Homing: approach retracted (home) with a slow final segment.
//   - Insert: move toward full insertion; slow in the last part of travel (contact region).
//   - Remove: retract to home.

Servo carriage;

// Wokwi diagram.json: green btn1 -> D3, blue btn2 -> D2, yellow btn3 -> D4; servo PWM -> D10.
const int PIN_BTN_HOME = 3;
const int PIN_BTN_INSERT = 2;
const int PIN_BTN_REMOVE = 4;
const int PIN_SERVO_PWM = 10;

const int ANGLE_HOME = 0;
const int ANGLE_INSERT = 180;
const int ANGLE_REMOVE = 45;

int currentAngle = ANGLE_HOME;

int prevHome = HIGH;
int prevInsert = HIGH;
int prevRemove = HIGH;

void ramp(int fromAngle, int toAngle, int steps, int delayMs);
void moveSegmented(int fromAngle, int toAngle, int fastSteps,
                   int fastDelayMs, int slowSteps, int slowDelayMs);
void motionHoming(void);
void motionInsert(void);
void motionRemove(void);

void setup() {
  carriage.attach(PIN_SERVO_PWM);
  carriage.write(currentAngle);

  pinMode(PIN_BTN_HOME, INPUT_PULLUP);
  pinMode(PIN_BTN_INSERT, INPUT_PULLUP);
  pinMode(PIN_BTN_REMOVE, INPUT_PULLUP);
}

void loop() {
  int h = digitalRead(PIN_BTN_HOME);
  int ins = digitalRead(PIN_BTN_INSERT);
  int rem = digitalRead(PIN_BTN_REMOVE);

  if (h == LOW && prevHome == HIGH) {
    motionHoming();
  }
  if (ins == LOW && prevInsert == HIGH) {
    motionInsert();
  }
  if (rem == LOW && prevRemove == HIGH) {
    motionRemove();
  }

  prevHome = h;
  prevInsert = ins;
  prevRemove = rem;
}

void motionHoming() {
  // Homing: reduced speed near the home position (limit switch stand-in).
  moveSegmented(currentAngle, ANGLE_HOME, 55, 8, 45, 22);
  currentAngle = ANGLE_HOME;
}

void motionInsert() {
  // Insert: optional slowdown near full depth — final fraction uses longer step delays.
  moveSegmented(currentAngle, ANGLE_INSERT, 40, 9, 45, 20);
  currentAngle = ANGLE_INSERT;
}

void motionRemove() {
  // Remove: steady retract to home (no extra “re-approach” ritual).
  ramp(currentAngle, ANGLE_REMOVE, 55, 12);
  currentAngle = ANGLE_REMOVE;
}

void moveSegmented(int fromAngle, int toAngle, int fastSteps,
                   int fastDelayMs, int slowSteps, int slowDelayMs) {
  if (fromAngle == toAngle) {
    return;
  }
  int mid = fromAngle + (toAngle - fromAngle) * 7 / 10;
  ramp(fromAngle, mid, fastSteps, fastDelayMs);
  ramp(mid, toAngle, slowSteps, slowDelayMs);
}

void ramp(int fromAngle, int toAngle, int steps, int delayMs) {
  if (fromAngle == toAngle || steps <= 0) {
    return;
  }
  float delta = toAngle - fromAngle;
  for (int i = 0; i <= steps; i++) {
    int angle = fromAngle + (int)(delta * i / (float)steps + 0.5f);
    carriage.write(angle);
    delay(delayMs);
  }
}
