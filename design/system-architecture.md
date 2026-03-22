## System Architecture – EMV Physical Card Inserter

### 1. Purpose and Context

The system automates **physical insertion and removal of EMV contact cards** into attended payment terminals for:

- EMV L3 certification scenarios.
- Local certification scenarios.
- Internal regression and pipeline automation.

**Demand traceability:** The system is designed to support **all EMV L3 and local certification scenarios** that require contact card insertion and removal. Scenario coverage is achieved by authoring tests (JUnit/Kotest or equivalent) that use the card inserter API for repeatable insert/remove sequences; the device does not implement EMV or terminal logic — it only provides reliable, repeatable physical motion on command.

The system **does not implement EMV or terminal business logic**. It only provides:

- Reliable, repeatable mechanical motion of a card into/out of the reader slot.
- A simple, REST over HTTPS API (JSON) for control.
- A JVM (Kotlin/Java) client API for use in automated tests.

The system is designed to be:

- **Terminal-agnostic**: reconfigurable fixtures/adapters for different terminal models.
- **Lab-network friendly**: controlled over HTTPS.
- **Tester-friendly**: clear, high-level commands and responses.

---

### 2. High-Level Decomposition

To match the priorities of **simple and low-cost assembly** and **a very simple API**, the system is decomposed into three main subsystems:

1. **Mechanical Subsystem**

   - Single-axis linear motion (one actuator) that moves the card in and out.
   - Simple, reusable fixtures/adapters for different terminal models.
   - Hard stops and minimal limit switches only where needed.
2. **Device Electronics and Firmware Subsystem (Primary: Ethernet-on-Device)**

   - **Primary design:** Microcontroller (STM32-class) with integrated Ethernet, connected directly to the lab network. The device runs the REST over HTTPS API and motion control on the same MCU (or behind a lab TLS gateway). No host daemon required.
   - Direct control of a single servo or linear actuator; 1–2 position sensors (home / fully inserted) as needed.
3. **Host Software Subsystem (JVM Client + Optional Daemon)**

   - Kotlin/Java client library:
     - Small, high‑level API: `insertCard()`, `removeCard()`, `status()`, `withInsertedCard { ... }`.
     - Connects to the device (or gateway) via HTTPS using the same REST API.
   - Optional simulator/fake device that implements the same REST API without hardware.

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

#### 3.2 Electronics Subsystem

**Responsibilities**

- Drive the actuator(s) with the required current/voltage and motion profile.
- Read sensors and provide reliable digital signals to the controller.
- Provide network connectivity to the lab: **primary** = Ethernet or Wi-Fi on device (REST/HTTPS on device or behind gateway).
- Provide power distribution and basic protections (fuses, reverse polarity protection as needed).

**Non-Responsibilities**

- No EMV or test flow logic.

#### 3.3 Firmware / Device Software

**Responsibilities**

- Implement the device state machine and motion sequences:
  - Homing on startup or on explicit command.
  - Controlled insertion to the configured depth and speed.
  - Controlled removal/retraction.
  - Transition to ERROR state on jams, sensor faults, and protocol misuse.
- Expose a **REST over HTTPS** API:
  - Endpoints for actions: home, insert, remove, status, abort, reset.
  - JSON request/response bodies with deterministic `OK` / `ERROR` results and error codes.
- Perform safety checks and enforce allowed state transitions.
- Log all commands, transitions, and errors with timestamps for debugging.

**Non-Responsibilities**

- No EMV transaction orchestration.
- No direct coupling to any particular test framework.

#### 3.4 JVM Client Library and Test Integration

**Responsibilities**

- Provide a **Kotlin-first, Java-friendly** API to:
  - Connect to the device over HTTPS (base URL).
  - Issue card movement commands via REST and handle JSON responses.
  - Represent device state and error conditions in JVM types.
- Hide the REST/JSON details from test authors.
- Provide integration points for:
  - JUnit/Kotest tests.
  - CI pipelines (e.g., running subsets of EMV L3/local scenarios).
- Optionally, implement a **simulated device**:
  - Same REST API surface.
  - In-memory state machine and fake timing.

**Non-Responsibilities**

- No direct hardware control.
- No responsibility for network/firewall configuration in the lab.

---

### 4. Deployment Topology

- One or more **Card Inserter Devices** on the lab network. Each device is addressed by **one HTTPS base URL** (e.g. `https://card-inserter-01.lab.local`):
  - **Primary:** The device has its own IP (Ethernet on MCU); the REST API is served over HTTPS (either on-device TLS or via a lab TLS gateway).
- EMV / certification tests run on developer machines, CI agents, or dedicated test controllers, and connect via the JVM client library to that base URL.

Example topology:

- `https://card-inserter-01.lab.local` – used by CI pipeline for standard regression.
- `https://card-inserter-02.lab.local` – used for manual experiments and local certification runs.

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

The REST `GET /api/status` endpoint exposes:

- Current state.
- Last error code and message (if any).
- Telemetry hints (e.g., last motion time).

The API additionally supports **asynchronous events** via Server-Sent Events (see `protocol-and-api-spec.md`) to notify clients about:

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
- Profiles are stored:
  - On the controlling side (client passes the relevant depth/speed in commands).

The first implementation may use a **single global configuration** (one terminal model) and evolve to multiple named profiles as needed.

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

Additional tools:

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

  - Only one logical test session should control a given device at a time. The JVM client or lab scheduler must implement locking/lease semantics so that concurrent test runs do not share the same device.
- **Emergency procedures**:

  - Presence of a physical E-stop accessible to the operator.
  - Clear documentation on how to recover from jams or ERROR state.

This document defines the **system-level architecture and responsibilities**. See:

- `protocol-and-api-spec.md` for the REST over HTTPS API and JVM API contracts.
- `mechanical-and-electronics-concept.md` for the physical implementation concept.
