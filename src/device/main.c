#include <Servo.h> 


Servo myservo;

// define the constants for each variable, being pin numbers and target angles

const int buttonPin1 = 2;
const int buttonPin2 = 3;
const int buttonPin3 = 4;

const int targetAngle1 = 75; 
const int targetAngle2 = 35;
const int targetAngle3 = 175; 

int currentAngle = 90; 

void setup() { // funciton to attach the servo motor to digital pin 10 and set the initial position.
  myservo.attach(10);
  myservo.write(currentAngle); // Set initial position
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
}

void loop() { 
  /* Continuously check the state of each push button. If a button is pressed (LOW state), 
  we call the moveToAngle function with the corresponding target angle. */

  if (digitalRead(buttonPin1) == LOW) {
    moveToAngle(targetAngle1);
  }
  if (digitalRead(buttonPin2) == LOW) {
    moveToAngle(targetAngle2);
  }
  if (digitalRead(buttonPin3) == LOW) {
    moveToAngle(targetAngle3);
  }
}

void moveToAngle(int targetAngle) { /* calculates the steps needed to move from the current angle to the target angle in 2 seconds. 
It then smoothly moves the servo motor to the target angle using a for loop and the write function. */
  int startAngle = currentAngle;
  int steps = 200; // Number of steps to take in 2 seconds
  float stepSize = (targetAngle - startAngle) / float(steps);

  for (int i = 0; i <= steps; i++) {
    int angle = startAngle + stepSize * i;
    myservo.write(angle);
    delay(10); 
  }
  currentAngle = targetAngle; // Update current angle
}