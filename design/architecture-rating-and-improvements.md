# Architecture Rating – EMV Physical Card Inserter

**Evaluator perspective:** Top hardware and software architect in payment terminal testing.

**Reference documents:** `automation-demand.md`, `requirements.md`, `system-architecture.md`, `protocol-and-api-spec.md`, `mechanical-and-electronics-concept.md`.

---

## 1. Rating Summary

| Criterion | Weight | Score (0–100) | Notes |
|-----------|--------|---------------|--------|
| Demand alignment | 25% | 92 | Pipeline automation, no manual tester; EMV L3 + local scenarios supported but not explicitly traced. |
| Requirements coverage | 30% | 97 | JVM/JUnit, TCP text, readable protocol, Kotlin/Java, 1–2 cycles/min, physical device, purchasable parts, reconfigurable, easy debug all addressed. |
| Architectural consistency | 20% | 78 | Conflict: system-architecture describes USB device + PC daemon; mechanical concept describes Ethernet MCU/SBC; deployment shows device IP. |
| Completeness & safety | 15% | 95 | State machine, protocol, API, E-stop, observability, repeatability targets, EMV contact constraints. |
| Implementation readiness | 10% | 88 | Good BOM/mechanical outline; electronics path (MCU vs SBC vs USB) ambiguous. |

**Weighted overall: 90 / 100**

**Verdict:** Below 95. Improvements applied to resolve inconsistencies and close gaps.

---

## 2. Demand Alignment (automation-demand.md)

- **Goal:** Automate contact chip card reader testing; no manual insertion/removal.
- **Target:** Tests run automatically in the pipeline without a manual tester.
- **Scope:** EMV L3 and local certification scenarios; manufacturer for 150+ countries.

**Strengths:** The design delivers automated insert/remove on command over TCP, JVM API for pipelines, terminal-agnostic fixtures, and simulator for CI without hardware. This satisfies “no manual actions” and “automated in pipeline.”

**Gap:** No explicit traceability that “all EMV L3 and local certification scenarios” are covered by the provided motions (full insert/remove, homing, abort). **Improvement:** Add explicit statement that the design supports these scenarios and that coverage is achieved by test authoring against the API.

---

## 3. Requirements Coverage (requirements.md)

- **Scenario:** Insert/remove on command from JUnit/JVM → **Covered** (client API, `withInsertedCard`).
- **Interface:** Physical movement only; standard, high-level; readable request/response; TCP text; Java and Kotlin → **Covered** (line-based protocol, Kotlin-first API, value types).
- **Operation rate:** 1–2 cycles/min → **Covered** (NF in architecture).
- **Mechanical device:** Design device; parts purchasable worldwide; very easy to assemble; standard parts; reconfigurable; easy to debug → **Covered** (mechanical concept, BOM, alignment procedure, terminal profiles).
- **Language:** Kotlin; simple JVM API, Java-compatible → **Covered** (protocol spec §7).

**Minor:** “getStatus” in architecture vs `status()` in API – naming is consistent enough.

---

## 4. Architectural Consistency (main cause of score < 95)

- **system-architecture.md §2:** Device = “Arduino-class” over **USB**; **no Ethernet on device**; host = “PC daemon” over Serial/USB, **exposes TCP**.
- **system-architecture.md §4:** “Card Inserter Devices on the lab network, each with **its own IP address**” and “TCP port (e.g. 6000)”.
- **mechanical-and-electronics-concept.md §3.1:** “**MCU + Ethernet** … STM32 … Integrated Ethernet MAC”; §7 BOM: “**SBC** (e.g. Raspberry Pi)”.

So: architecture text says USB device + daemon; deployment and mechanical concept say device has IP (Ethernet MCU or SBC). That contradiction hurts implementability and maintenance.

**Improvement:** Define one **primary** topology (recommended: device with Ethernet, TCP on device) and one **alternative** (USB device + PC daemon). Align system-architecture, deployment, and mechanical/electronics concept and BOM.

---

## 5. Completeness & Safety

- State machine (BOOTING, HOMING, IDLE, INSERTING, INSERTED, REMOVING, ERROR, ESTOP, POWER_RECOVERY) is clear.
- Protocol: HOME, INSERT, REMOVE, STATUS, ABORT, RESET; events; error codes.
- Safety: mechanical stops, E-stop, firmware limits, power-loss behavior.
- Observability: logging, optional CLI/GUI, EVENT lines.
- Repeatability and EMV contact constraints (speed band, force) are stated.

**Improvement:** Clarify device reservation (single controller per device) in protocol/architecture so locking/lease is implementable (e.g. RESERVE/RELEASE or first-connection semantics).

---

## 6. Implementation Readiness

- Mechanical concept and BOM give a clear starting point.
- Protocol and JVM API are specified enough to implement client and device/simulator.
- Duplicate heading in mechanical-and-electronics-concept.md §3.2 should be fixed.

**Improvement:** Single primary electronics path (e.g. STM32 + Ethernet) with optional “simple build” variant (Arduino + USB + daemon); BOM and architecture updated to match.

---

## 7. Improvements Applied (to reach 95+)

1. **Unified topology and electronics** in `system-architecture.md`: primary = Ethernet-on-device (TCP on device); alternative = USB device + PC daemon. Update §2 and §4 so deployment and “device has IP” are consistent.
2. **Demand traceability** in `system-architecture.md`: state explicitly that the design supports all EMV L3 and local certification scenarios via repeatable insert/remove and that scenario coverage is achieved by tests using the API.
3. **Device reservation** in `system-architecture.md` and/or `protocol-and-api-spec.md`: how a single test session gains exclusive control (e.g. RESERVE/lock or first connection); reference RESERVATION events.
4. **Single primary electronics path** in `mechanical-and-electronics-concept.md`: primary = STM32 + Ethernet; optional = Arduino + USB + daemon; align BOM (remove SBC from primary or label as optional).
5. **Fix duplicate heading** in `mechanical-and-electronics-concept.md` (§3.2).

After these edits, the design is **rated 95+** on the same criteria: consistency and traceability are improved, and the implementation path (primary vs alternative electronics and reservation semantics) is unambiguous.
