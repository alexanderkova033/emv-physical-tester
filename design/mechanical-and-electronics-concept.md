## Mechanical and Electronics Concept

### 1. Objectives

- Provide a **repeatable, robust mechanical mechanism** for inserting/removing EMV cards into/from standard attended payment terminals.
- Use **globally available, standard components**.
- Make the device **simple to assemble, service, and reconfigure** for different terminal models.

This document describes a **reference concept**; exact implementation can be adapted to local sourcing and manufacturing capabilities.

---

### 2. Mechanical Concept

#### 2.1 Overall Layout

- The device consists of:
  - A **base frame** (e.g., aluminum extrusion) that sits on a bench.
  - A **terminal fixture** that holds the payment terminal in a consistent orientation.
  - A **linear axis** that moves a card carriage into and out of the card reader slot.

Orientation:
- Default configuration assumes a **horizontal insertion** along the card slot axis.
- Alternative orientations (e.g., slight upward or downward angle) are possible via adjustments in the fixture.

#### 2.2 Base Frame and Terminal Fixture

- Frame:
  - Use standard aluminum extrusion profiles (e.g., 20x20 or 30x30 mm).
  - Connect with standard corner brackets and T-nuts.
  - Provide a flat reference surface for the terminal and linear axis.

- Terminal fixture:
  - Adjustable along at least:
    - X-axis: horizontal offset relative to the card carriage.
    - Y-axis: vertical height to align slot center.
    - θ (rotation) around vertical axis for angle fine-tuning.
  - Achieved via slotted brackets, shims, or sliding plates with locking screws.
  - Interchangeable “terminal plates” for different models:
    - Each plate matches the terminal’s bottom contour and mounting holes (if any).
    - Plates can be swapped without changing the rest of the mechanism.

#### 2.3 Linear Axis and Card Carriage

- Linear axis:
  - Single-axis motion using:
    - Option A: Lead screw + nut + linear guide block.
    - Option B: Belt drive + linear guide (slightly higher speed, similar complexity).
  - Travel length: sufficient to move from a **retracted home** position to **full insertion** with margin.
  - Components:
    - Linear rail (or rod+bearing) with carriage block.
    - Lead screw/belt assembly.
    - NEMA 17 (or similar) stepper motor or DC gearmotor.

- Card carriage:
  - Holds a standard EMV card (and possibly variances in thickness) securely.
  - Feature ideas:
    - Spring-loaded clip or clamp.
    - Card locator edges to ensure known reference (e.g., align along one corner).
  - The front edge of the card is aligned with the axis such that:
    - When the carriage reaches a specific travel, the card chip lands in the contact area.

#### 2.4 Stops, Sensors, and Safety

- Mechanical stops:
  - Hard stops at both ends of travel to prevent overrun.
  - Stops should be robust enough to withstand misconfiguration.

- Sensors:
  - Home sensor:
    - E.g., mechanical limit switch or optical endstop at retracted position.
  - Forward/end-of-travel sensor:
    - To detect unexpected overtravel or jam.
  - Optional card-present sensor:
    - Near the card slot, optical or mechanical, to detect whether the card actually entered the slot.

- Safety considerations:
  - Motion speed limited such that contact forces with the terminal are within safe limits.
  - Homing routine uses reduced speed near sensors.
  - Firmware always respects configured min/max positions.

Quantitative guidance (initial targets):
- Insertion depth tolerance:
  - Repeatability: ±0.3 mm at the card edge after a homing cycle.
  - Absolute error relative to ideal contact position: within ±0.5 mm given proper alignment.
- Angular tolerances:
  - Yaw/pitch misalignment: ±1° at full insertion.
- Insertion speed:
  - Typical configurable range: 50–150 mm/s through the contact region (to be tuned per terminal and EMV recommendations).

---

### 3. Electronics Concept

#### 3.1 Control Architecture (Primary Choice)

To better match an industrial, VERIFONE-grade lab tool, the **primary reference design** uses:

- **MCU + Ethernet as the motion controller (authoritative source of truth)**
  - Microcontroller: STM32-class MCU with:
    - Integrated Ethernet MAC + external PHY.
    - Sufficient flash/RAM for motion control + TCP server.
  - Responsibilities:
    - Real-time motion control and sensor handling.
    - Direct implementation of the TCP protocol and state machine.

Optionally, a **secondary SBC-based gateway** can be added later on top for:
- Aggregating logs from multiple devices.
- Hosting dashboards or higher-level automation tools.

This choice:
- Ensures deterministic real-time behavior and robustness.
- Avoids SD-card/OS issues on the critical motion-control path.
- Keeps the protocol and behavior stable across labs.

#### 3.2 Core Electronic Components

#### 3.2 Core Electronic Components

- Power supply:
  - 24 VDC (typical for stepper systems) or other appropriate voltage based on motor choice.
  - Sizing for peak current of motor plus SBC.

- Motor driver:
  - Stepper driver module (e.g., common off-the-shelf module) rated for the motor current.
  - Current limiting configured to protect the mechanism.

- Sensors:
  - Limit/home switches (mechanical or optical).
  - Optional card-present sensor.
  - All connected to digital inputs with appropriate debouncing/protection.

- Network interface:
  - Ethernet port from SBC/MCU.
  - Optionally, a status LED to show link/activity and device state.

---

### 4. Firmware / Motion Control Concept

The firmware (on SBC or MCU) is responsible for:
- Executing motion profiles for:
  - Homing.
  - Full insert.
  - Full remove.
- Monitoring sensors and aborting motion on unsafe conditions.
- Exposing the TCP protocol described in `protocol-and-api-spec.md`.

Motion sequences (conceptual):

- Homing:
  - Move slowly towards home switch until triggered.
  - Back off slightly, re-approach for precision.
  - Set position counter to zero at home.

- Insert:
  - From home, move forward to target depth (converted from mm to steps).
  - Optionally, slow down near the final contact region.
  - If forward sensor or overcurrent/jam condition, abort and signal error.

- Remove:
  - From inserted position, move back to home, respecting sensor feedback.

All distances and speeds are **configurable parameters**, either in firmware or via configuration files.

Power-loss and E-stop behavior:
- On power loss:
  - The mechanical system must be designed so that loss of motor torque does not cause uncontrolled further insertion.
  - On next power-up, firmware must assume a conservative state (e.g., card may still be in reader) and require a controlled recovery motion.
- E-stop:
  - A physical E-stop input (normally closed loop) should cut motor power and signal the controller.
  - With E-stop asserted, the firmware:
    - Rejects all motion commands with `ESTOP_ASSERTED`.
    - Continues to serve `STATUS` so clients can see the fault.

---

### 5. Configuration for Different Terminal Models

To support multiple terminals:
- Define a set of configuration parameters per terminal model:
  - `insertion_depth_mm`
  - `nominal_speed_mm_s`
  - `retract_speed_mm_s`
  - Physical offset from base reference (for assembly/fixture).
- Storage options:
  - On-device configuration files (e.g., JSON/YAML).
  - Or external configuration on the controller that generates appropriate `INSERT` command parameters.

In the first phase, a **single configuration** for one terminal model may be hardcoded to reduce complexity.

---

### 6. Assembly and Serviceability

- Target:
  - Assembly by a technician using:
    - Standard hand tools (hex keys, screwdriver).
    - Simple alignment procedure documented in an assembly guide.

- Alignment procedure (high level):
  1. Install frame and linear axis.
  2. Mount terminal plate and terminal.
  3. Use a reference card and jogging tool (via test software) to adjust:
     - X/Y position.
     - Angular alignment.
  4. Tighten all brackets once alignment is satisfactory.

- Service:
  - Wear components (belts, bearings, motor coupler) are replaceable without disassembling the entire device.
  - Sensors are accessible for replacement and inspection.

Design-for-manufacture/assembly (DFM/DFA) checklist (high level):
- Prefer:
  - Single fastener type/size where possible.
  - Symmetric parts to reduce assembly errors.
  - Keyed/bracket features that naturally align components.
- Avoid:
  - Hidden fasteners that require partial disassembly of unrelated modules.
  - Overly tight tolerances that exceed common workshop capabilities without clear justification.

---

### 7. Example Initial BOM Outline (Conceptual)

This is a conceptual list; specific part numbers depend on region and supplier.

- Frame:
  - 20x20 mm aluminum extrusion, cut lengths.
  - Corner brackets, T-nuts, screws.

- Linear axis:
  - Linear rail with carriage block (or linear rod + linear bearings).
  - Lead screw + nut + coupler (or timing belt + pulleys).
  - NEMA 17 stepper motor (or equivalent).

- Fixtures:
  - Terminal plates (custom, made per model).
  - Slotted brackets for adjustment.

- Sensors:
  - 2x limit switches (home, forward).
  - Optional optical card-present sensor.

- Electronics:
  - SBC (e.g., industrial Raspberry Pi-equivalent).
  - Stepper motor driver module.
  - 24 VDC power supply (or suitable for chosen motor).
  - Wiring, connectors, terminal blocks, fuses as required.

This concept document is intentionally high-level; it is meant to guide detailed CAD, schematic, and firmware design while remaining aligned with the overall system and protocol architecture.

