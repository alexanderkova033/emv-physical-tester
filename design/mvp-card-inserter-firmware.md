# MVP Card Inserter Firmware (Arduino / Wokwi)

This document describes the **minimum viable** card-inserter firmware on the `mvp` branch: an Arduino Uno–style target, push-button “API” stand-ins, a single hobby servo, and a [Wokwi](https://wokwi.com/) project for interactive simulation.

The long-term product is defined in `system-architecture.md` and `protocol-and-api-spec.md` (REST over HTTPS, Ethernet, richer safety). The MVP deliberately narrows scope to **prove the core state machine and motion policy** against hardware (or the emulator) without network stack or mechanical fixture details.

---

## What the MVP delivers

- **Domain core (`DeviceController`)** – States, legal commands, insert/home/remove/abort, E-stop handling, segmented servo motion with cooperative delays so **status, abort, and E-stop stay responsive during moves**.
- **Ports (`DevicePorts`)** – Framework-agnostic hooks for time, delays, servo, E-stop, and logging. The Arduino layer implements these; another platform could swap the adapter without rewriting the state machine.
- **Lab-style inputs** – Five buttons map to the same *logical* operations as key REST endpoints (see pin table). Serial output uses labels like `POST /api/insert` so traces align mentally with the full protocol spec.
- **Serial presentation** – State-change lines (`data: {"type":"STATE_CHANGED",...}`), reservation events, structured errors, and a **status JSON** shaped like a successful `GET /api/status` response (simplified fields).

What is **out of scope** for this MVP (may appear in types or placeholder status flags but are not fully wired or lab-tested here): HTTP server, TLS, reservation GPIO lines, dedicated reset button, SSE over TCP, jam/sensor faults, and multi-terminal profiles.

---

## Repository layout

| Path | Role |
|------|------|
| `emv_physical_tester/card_inserter/firmware/` | C++ sources and `card_inserter_firmware_sketch.ino` (Arduino `setup` / `loop` entry points). |
| `emulator/diagram.json` | Wokwi circuit: Uno, servo on D10, five buttons on D2–D6. |
| `emulator/libraries.txt` | Declares the **Servo** library for Wokwi. |

---

## Pin assignments (MVP)

Defined in `button_board_pins.h`. Active-low buttons with internal pull-ups (press = LOW).

| Pin | Logical operation | Notes |
|-----|-------------------|--------|
| D2 | `POST /api/insert` | Uses default depth/speed from `card_inserter_firmware_app.cpp`. |
| D3 | `POST /api/home` | |
| D4 | `POST /api/remove` | |
| D5 | `GET /api/status` | Rising edge prints one JSON status line on Serial. |
| D6 | `POST /api/abort` | No-op if not in `HOMING` / `INSERTING` / `REMOVING`. |
| D10 | Servo PWM | |
| D12 | E-stop | LOW = asserted. If asserted at boot, device enters `ERROR` (`ERR_ESTOP`). |

Comments in the header reference additional pins reserved in the full design (reset, events, reserve/release).

---

## Default motion parameters

Configured in `card_inserter_firmware_app.cpp` (tune for your mechanism):

- Servo angles: home `0°`, remove `30°`, insert `152°`.
- Depth: max `50` mm, default insert depth `35` mm; default speed `20` mm/s (mapped to ramp timing in the controller).

---

## Build and flash (hardware)

1. Open `card_inserter_firmware_sketch.ino` in Arduino IDE 2.x (or use your usual Arduino CLI workflow).
2. Select **Arduino Uno** (or compatible) and the correct serial port.
3. Ensure the **Servo** library is available (IDE bundled library is sufficient).
4. Compile and upload. Open Serial Monitor at **9600 baud**.

If the sketch is opened from a copy that only contains the `.ino` file, copy the entire `.cpp` / `.h` tree from this repository so all translation units are present.

---

## Wokwi emulator

1. Create or open a Wokwi Arduino Uno project.
2. Replace the project’s diagram with `emulator/diagram.json` (or merge parts/connections as needed).
3. Add libraries from `emulator/libraries.txt` (at minimum **Servo**).
4. Upload or paste the firmware sources so Wokwi builds the same sketch as above.

Use the on-canvas buttons to drive insert/home/remove/status/abort. Open the serial monitor in Wokwi to see command traces and status JSON.

---

## Runtime behavior (short)

- **Boot** – Hardware init, `DeviceController` starts from `BOOTING` and follows the same transitions as the design state machine for home/idle/insert/removal paths.
- **E-stop** – `OnEstop()` runs at the top of `loop` and during motion delay slices; asserted E-stop forces `ERROR` with `ERR_ESTOP`.
- **Abort** – Requests early exit from active motion; behavior is implemented inside `DeviceController` (ramp segmentation and abort flags).

`Reset()`, `Reserve()`, and `Release()` exist on `DeviceController` but are **not** connected to the MVP button adapter; recovery from certain errors may require a controller reset or future wiring—see protocol spec for intended semantics.

---

## Debug defines

- **`DEBUG_ERR_CHAR_MS`** (in `card_inserter_firmware_app.cpp`, overridable at build time) – If non-zero, multi-line error text is typed slowly on Serial (character delay in milliseconds); `0` prints errors immediately.

---

## Relationship to the full protocol

The MVP is a **deliberate subset**: physical buttons instead of REST, Serial instead of HTTPS JSON bodies, and a minimal status document. When extending toward Phase 2+ in `implementation-roadmap-and-acceptance-tests.md`, keep `DeviceController` and `DevicePorts` as the stable core and add a network/command adapter that maps HTTP routes to the same use-case entry points (`Home`, `Insert`, `Remove`, `Abort`, `Reset`, etc.).
