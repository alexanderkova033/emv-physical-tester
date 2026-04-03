# emv-physical-tester

Technical design and reference artifacts for an automated EMV physical card inserter used to test contact chip readers on attended payment terminals.

## MVP branch (firmware + emulator)

This repository includes a **minimal Arduino implementation** of the card-inserter core state machine plus a **Wokwi** circuit for interactive simulation:

- **Firmware:** `emv_physical_tester/card_inserter/firmware/` – `DeviceController` domain logic, `DevicePorts` boundary, Arduino servo/button adapters, and `card_inserter_firmware_sketch.ino`.
- **Emulator:** `emulator/diagram.json` and `emulator/libraries.txt`.

Documentation for scope, pins, build steps, and how the MVP maps to the full REST design:

- `design/mvp-card-inserter-firmware.md`

## Design reference (`design/`)

- `system-architecture.md` – overall hardware/firmware/software architecture and responsibilities.
- `protocol-and-api-spec.md` – REST over HTTPS API and JVM (Kotlin/Java) client API.
- `mechanical-and-electronics-concept.md` – mechanical layout, actuator concept, and electronics approach.
- `implementation-roadmap-and-acceptance-tests.md` – phased delivery and conformance expectations.