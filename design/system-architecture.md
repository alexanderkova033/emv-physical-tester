## System Architecture – EMV Physical Card Inserter

### 1. Purpose and Context

The system automates **physical insertion and removal of EMV contact cards** into attended payment terminals for:
- EMV L3 certification scenarios.
- Local certification scenarios.
- Internal regression and pipeline automation.

The system **does not implement EMV or terminal business logic**. It only provides:
- Reliable, repeatable mechanical motion of a card into/out of the reader slot.
- A simple, TCP-based text protocol for control.
- A JVM (Kotlin/Java) client API for use in automated tests.

The system is designed to be:
- **Terminal-agnostic**: reconfigurable fixtures/adapters for different terminal models.
- **Lab-network friendly**: controlled over TCP/IP.
- **Tester-friendly**: clear, high-level commands and responses.

---

### 2. High-Level Decomposition

The system is decomposed into four main subsystems:

1. **Mechanical Subsystem**
   - Card carriage and actuator (linear movement).
   - Adjustable fixtures to hold different terminal models.
   - Hard stops and limit switches.

2. **Electronics Subsystem**
   - Motor drivers (stepper or DC gearmotor + driver).
   - Sensor inputs (home, end-of-travel, optional card-present).
   - **Primary controller: MCU with Ethernet (STM32-class)**.
   - Optional higher-level SBC gateway (non-critical for motion).
   - Power supply and basic protections.

3. **Firmware / Device Software**
   - Motion control and safety interlocks.
   - Device state machine (IDLE, HOMING, READY, INSERTING, INSERTED, REMOVING, ERROR).
   - TCP server implementing the text protocol.
   - Logging and diagnostics.

4. **JVM Client Library and Test Integration**
   - Kotlin library (Java-compatible) exposing a simple API:
     - `home()`, `insertCard()`, `removeCard()`, `status()`, `withCardInserted { ... }`, etc.
   - Optional simulator / fake device for development without hardware.
   - Integration helpers for JUnit/Kotest and CI.

5. **Operational and Safety Layer**
   - Lab operating procedures, device reservation/locking.
   - Emergency stop handling and power-loss behavior.
   - Factory/acceptance tests and maintenance procedures.

---

### 3. Responsibilities and Boundaries

#### 3.1 Mechanical Subsystem

**Responsibilities**
- Move a real EMV card along a controlled path into/out of the terminal’s card reader slot.
- Provide accurate, repeatable card position (home, inserted, retracted).
- Provide mechanical alignment and adjustment to support different terminal geometries.
- Provide physical safety: hard stops preventing over-insertion, mechanical robustness.

**Non-Responsibilities**
- No knowledge of EMV states, transactions, or application flows.
- No network or protocol logic.

#### 3.2 Electronics Subsystem

**Responsibilities**
- Drive the actuator(s) with the required current/voltage and motion profile.
- Read sensors and provide reliable digital signals to the controller.
- Provide network connectivity (Ethernet) to the lab network.
- Provide power distribution and basic protections (fuses, reverse polarity protection as needed).

**Non-Responsibilities**
- No high-level command parsing (delegated to firmware).
- No EMV or test flow logic.

#### 3.3 Firmware / Device Software

**Responsibilities**
- Implement the device state machine and motion sequences:
  - Homing on startup or on explicit command.
  - Controlled insertion to the configured depth and speed.
  - Controlled removal/retraction.
  - Transition to ERROR state on jams, sensor faults, and protocol misuse.
- Expose a **simple, line-based text protocol over TCP**:
  - Accept commands such as `HOME`, `INSERT`, `REMOVE`, `STATUS`, `ABORT`.
  - Respond with deterministic `OK` / `ERROR` messages including error codes.
- Perform safety checks and enforce allowed state transitions.
- Log all commands, transitions, and errors with timestamps for debugging.

**Non-Responsibilities**
- No EMV transaction orchestration.
- No direct coupling to any particular test framework.

#### 3.4 JVM Client Library and Test Integration

**Responsibilities**
- Provide a **Kotlin-first, Java-friendly** API to:
  - Connect to the device over TCP/IP.
  - Issue card movement commands and handle their responses.
  - Represent device state and error conditions in JVM types.
- Hide the text protocol details from test authors.
- Provide integration points for:
  - JUnit/Kotest tests.
  - CI pipelines (e.g., running subsets of EMV L3/local scenarios).
- Optionally, implement a **simulated device**:
  - Same protocol surface.
  - In-memory state machine and fake timing.

**Non-Responsibilities**
- No direct hardware control.
- No responsibility for network/firewall configuration in the lab.

---

### 4. Deployment Topology

- One or more **Card Inserter Devices** on the lab network, each with:
  - Its own IP address.
  - A configured TCP port for control (e.g., `6000`).
- EMV / certification tests run on:
  - Developer machines.
  - CI agents.
  - Dedicated test controllers.
  They connect via the client library to the devices.

Example topology:
- `card-inserter-01.lab.local:6000` – used by CI pipeline for standard regression.
- `card-inserter-02.lab.local:6000` – used for manual experiments and local certification runs.

---

### 5. Device State Model (Logical)

The firmware exposes a logical state machine. Key states:

- `BOOTING` – Initial boot, self-check.
- `HOMING` – Moving to reference/home position.
- `IDLE` – Homed and ready but no card movement in progress.
- `INSERTING` – Moving card into the terminal slot.
- `INSERTED` – Card is fully inserted and held in place.
- `REMOVING` – Retracting card from slot.
- `ERROR` – Jam detected, sensor failure, or protocol misuse.

Transitions are triggered by:
- Protocol commands (e.g., `HOME`, `INSERT`, `REMOVE`).
- Internal events (sensor trigger, timeout, overcurrent).

Additional safety-related states/events:
- `ESTOP` – Emergency stop asserted (physical E-stop or safety input). Motion is disabled until reset.
- `POWER_RECOVERY` – Optional transient state after power-up if the card may still be in the reader.

Recovery rules (high level):
- From `ERROR`:
  - `RESET` (or `HOME` if configured) attempts to move to a known safe state (`IDLE`), if sensors allow.
  - If the device cannot move safely, it remains in `ERROR` and reports diagnostics.
- From `ESTOP`:
  - Motion is disabled; only `STATUS` is processed.
  - Releasing E-stop and issuing `RESET` returns to `BOOTING`/`HOMING` sequence.
- From `POWER_RECOVERY`:
  - Firmware infers a conservative state (e.g., assume card is still present) and requires an explicit recovery motion (implementation detail documented in firmware spec).

The TCP `STATUS` command exposes:
- Current state.
- Last error code and message (if any).
- Telemetry hints (e.g., last motion time).

The protocol additionally supports **asynchronous events** (see `protocol-and-api-spec.md`) to notify clients about:
- State changes.
- Faults (e.g., E-stop, sensor fault, jam).
- Reservation/lock ownership changes.

---

### 6. Configuration and Terminal Models

To support multiple payment terminal models:
- Each terminal model has a **configuration profile**:
  - Insertion depth (mm).
  - Insertion speed profile (mm/s).
  - Dwell behavior (whether card is held in place or released).
  - Physical offsets relative to the mechanical reference.
- Profiles are stored either:
  - On the device (e.g., configuration files or firmware parameters).
  - Or on the controlling side (client passes the relevant depth/speed in commands).

The first implementation may use a **single global configuration** (one terminal model) and evolve to multiple named profiles as needed.

Terminal profiles are identified by a **profile ID** (e.g., `VX820`, `V400C`) that:
- Is referenced mechanically in fixture documentation.
- May be exposed via protocol/API (e.g., optional `profile_id` field in `STATUS`).

Configuration must ensure that:
- Requested `depth_mm` and `speed_mm_s` from the protocol:
  - Fall within safe bounds for the configured terminal model.
  - Are clamped or rejected with a clear error if out of range.

---

### 7. Observability and Diagnostics

The system must be easy to diagnose:
- **On-device logs** (or remote logs) including:
  - Command received (with correlation ID).
  - State transitions.
  - Errors and sensor anomalies.
- **Client-side logs**:
  - Connection lifecycle.
  - Command invocations with timing.
  - Mapping from test cases to device commands.

Additional tools (future work):
- A small CLI or GUI utility to:
  - Connect to the device.
  - Send manual commands (HOME/INSERT/REMOVE).
  - View status and recent logs.

The protocol can emit **asynchronous `EVENT` lines** (see `protocol-and-api-spec.md`) that:
- Provide live state change and fault information without polling.
- Can be consumed by lab dashboards and CI logs.

---

### 8. Non-Functional Requirements (NF)

- **Reliability**: Designed for thousands of cycles per day at 1–2 cycles/minute.
- **Serviceability**: Easy for technicians to:
  - Replace wear parts (belts, bearings, motor).
  - Recalibrate home position and depth.
- **Repeatability**:
  - Insertion depth repeatability target: within ±0.3 mm at the card edge after homing.
  - Lateral/vertical misalignment target: within ±0.5 mm relative to slot centerline.
  - Angular misalignment (yaw/pitch): within ±1° at full insertion.
- **Safety**:
  - Mechanical stops and limit switches prevent damage to terminals or device.
  - Firmware enforces motion ranges and monitors errors.
  - Emergency stop input and conservative behavior on power loss.
- **Extensibility**:
  - Ability to introduce additional commands (e.g., partial insert, “wiggle”) without breaking existing tests.

---

### 9. EMV / Contact-Interface-Specific Constraints (Guidance)

The mechanism must respect typical EMV/contact reader physical constraints, including:
- Controlled insertion speed band (e.g., 50–150 mm/s through the contact region, tunable per terminal).
- Maximum normal force on the contacts (controlled via speed/current limits and mechanical compliance).
- Avoidance of excessive vibration or oscillation at the contact region.

These constraints inform:
- Motion profile design in firmware.
- Torque/current limits in motor driver configuration.
- Mechanical tolerances and compliance elements in the design.

---

### 10. Operational and Safety Layer Overview

Operational aspects (to be elaborated in procedures outside this document):
- **Device reservation**:
  - Only one logical test session should control a given device at a time (locking/lease semantics in JVM client or via a lab scheduler).
- **Emergency procedures**:
  - Presence of a physical E-stop accessible to the operator.
  - Clear documentation on how to recover from jams or ERROR state.
- **Acceptance testing**:
  - Each device must pass a defined factory / acceptance test script (subset of EMV-representative motions) before being admitted to a lab.

These aspects close the gap between pure architecture and real lab operation for a VERIFONE-grade tool.

This document defines the **system-level architecture and responsibilities**. See:
- `protocol-and-api-spec.md` for the TCP protocol and JVM API contracts.
- `mechanical-and-electronics-concept.md` for the physical implementation concept.

