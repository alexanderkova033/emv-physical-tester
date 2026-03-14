## Implementation Roadmap and Acceptance Tests

### 1. Purpose

This document turns the architecture into a concrete plan and defines how to **prove** that a device instance is ready for EMV and local certification testing.

---

### 2. Implementation Roadmap (Phased)

#### Phase 1 – Protocol, API, and Simulator

- Deliverables:
  - Reference implementation of:
    - TCP protocol parser/serializer.
    - JVM client (`CardInserterClient` and related types).
  - Deterministic simulator implementing the same protocol and state machine.
- Success criteria:
  - All protocol/API unit and integration tests pass against the simulator.
  - EMV test teams can write and run basic “simulated card insertion” tests in CI.

#### Phase 2 – First Mechanical/Electronics Prototype (Single Terminal Model)

- Deliverables:
  - Single-axis mechanism and fixture for a chosen reference terminal model.
  - Electronics board with STM32-class MCU, Ethernet, motor driver, sensors, and power.
  - Basic firmware:
    - Homing.
    - INSERT/REMOVE sequences.
    - STATUS and ERROR reporting.
- Success criteria:
  - Repeated insertion/removal at 1–2 cycles/min for at least 5,000 cycles without mechanical failure.
  - Measured insertion depth and alignment within the specified tolerances.

#### Phase 3 – Firmware Feature Completion and Safety

- Deliverables:
  - Full state machine including:
    - ERROR, ESTOP, POWER_RECOVERY handling.
    - `RESET` and `ABORT` semantics as in protocol spec.
  - Asynchronous `EVENT` support (state changes, faults).
  - Configuration profiles for the reference terminal model.
- Success criteria:
  - Firmware passes the **protocol/state conformance test suite** (see below).
  - All safety behaviors verified: E-stop, sensor faults, jams, and power loss.

#### Phase 4 – Multi-Terminal Support and Lab Integration

- Deliverables:
  - Mechanical fixtures and configuration profiles for additional terminal models.
  - Lab integration:
    - Device discovery/documentation (IP, profile IDs).
    - CI jobs targeting at least one real device.
- Success criteria:
  - EMV L3 and local certification flows can run end-to-end without manual card handling in at least one lab.
  - All supported terminal models pass their mechanical/acceptance tests.

---

### 3. Protocol and Firmware Conformance Tests

For each firmware build, run a conformance suite (automated from JVM side):

- **State machine tests**
  - Valid sequences: `BOOTING → HOMING → IDLE → INSERTING → INSERTED → REMOVING → IDLE`.
  - Illegal commands in each state (e.g., `INSERT` during `HOMING`) must return `ILLEGAL_STATE`.
  - `RESET` correctly recovers from `ERROR` where possible.

- **Error and fault tests**
  - Simulated `CARD_JAM`, `MOTION_TIMEOUT`, `SENSOR_FAULT`:
    - Correct error codes and state transitions.
    - No unsafe additional motion after fault detection.
  - E-stop asserted:
    - All motion commands rejected with `ESTOP_ASSERTED`.
    - `STATUS` continues to work.

- **Versioning and feature tests**
  - `STATUS` returns `protocol_version`, `min_compatible_protocol_version`, and `features`.
  - Client behaves correctly if:
    - Device has a newer protocol with extra fields.
    - Device is older than client’s minimum supported version (test for graceful refusal).

---

### 4. Mechanical and Tolerance Acceptance Tests

For each physical device:

- **Geometric checks**
  - Using gauges or measurement tools:
    - Verify insertion depth at full stroke:
      - Mean depth within ±0.5 mm of target.
      - Standard deviation over 20 cycles suggests repeatability within ±0.3 mm.
    - Verify lateral and vertical offsets relative to slot centerline within ±0.5 mm.
    - Verify angular misalignment (yaw/pitch) within ±1°.

- **Cycle test**
  - Run at least 10,000 insert/remove cycles at 1–2 cycles/min:
    - No mechanical failures.
    - No increase in misalignment or unacceptable wear.

- **Terminal profile validation**
  - For each supported terminal profile (e.g., `VX820`, `V400C`):
    - Load the configuration.
    - Run a short sequence of operations and verify contact engagement (reader detects card reliably).

---

### 5. Safety and Operational Acceptance Tests

- **E-stop and power-loss**
  - Assert E-stop during:
    - HOMING.
    - INSERTING.
    - INSERTED (holding).
  - Verify:
    - Motion stops safely.
    - Commands return `ESTOP_ASSERTED`.
    - After releasing E-stop and issuing `RESET`, device returns to `IDLE` without over-insertion.
  - Power cycle during INSERTING:
    - On next boot, firmware behaves conservatively:
      - Does not blindly retract without checking sensors.
      - Requires explicit recovery command per firmware spec.

- **Reservation / lab use model (if implemented)**
  - Acquire a logical reservation (from JVM client or scheduler).
  - Ensure:
    - Only the owner can send motion commands.
    - Other clients see clear errors or `RESERVATION` events.

---

### 6. EMV/Certification Scenario Validation (Representative)

For each EMV L3 and local certification suite, define a minimal set of **representative physical scenarios**:

- Example categories:
  - Normal successful transaction (single insert/removal).
  - Card removed mid-transaction (simulated premature removal).
  - Declined/retry flows with multiple insertions.

For each category:
- Map existing manual card actions to device commands (`INSERT`, `REMOVE`, timing windows).
- Validate:
  - The reader and EMV stack behave as expected.
  - Timing jitter introduced by automation is within acceptable limits.

Once these representative scenarios pass reliably on the automated setup, you can phase out equivalent manual tests.

---

This roadmap and acceptance-test definition are intended to close the gap between a high-level architecture and a **deployable, repeatable solution** across labs. 

