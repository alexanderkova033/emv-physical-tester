#include "card_inserter_app.h"

// Some build environments compile this folder as the Arduino "sketch" and do not
// pick up the repository-root `sketch.ino`. Providing `setup()` / `loop()` here
// avoids link errors like "undefined reference to `setup`/`loop`".

void setup() { cardInserterApp_setup(); }

void loop() { cardInserterApp_loop(); }

