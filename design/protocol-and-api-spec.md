## TCP Protocol and JVM API Specification

### 1. Goals

- Provide a **simple, readable text protocol** over TCP/IP to control the card inserter.
- Ensure the protocol is **easy to debug manually** (via netcat/telnet-like tools).
- Provide a **type-safe Kotlin API** that is also easy to use from Java-based tests.

The protocol is **request–response, line-oriented**, with one response per command.

---

### 2. Connection Model

- Transport: **TCP**.
- Default port: **6000** (configurable in firmware).
- Encoding: **UTF-8**.
- Line terminator: `\n` (LF). `\r\n` SHOULD also be accepted by the device.
- Client initiates the connection; device accepts multiple connections but processes **commands sequentially per connection**.

Timeouts:
- Client-side **command timeout** is configurable in the JVM API (e.g. default 10 seconds).
- Device may disconnect idle clients after a configurable idle period.

Asynchronous events:
- The device MAY send **unsolicited, asynchronous `EVENT` lines** to clients to report:
  - State changes.
  - Faults (e.g., E-stop, sensor fault).
  - Reservation/lock changes.
- Each event is a single line and does not break the request–response model (clients must be able to distinguish `RESULT` from `EVENT`).

---

### 3. Message Format

#### 3.1 Requests (Commands)

- One command per line.
- Format:

  `<COMMAND_NAME> [arg1=value1 arg2=value2 ...] [# optional comment]`

- Command name: uppercase ASCII letters + optional underscores.
- Arguments:
  - Key: lowercase letters and underscores.
  - Value: string without whitespace or `#`. If a value requires spaces, it can be quoted, but the initial version should avoid that.
- Anything after `#` is a comment and ignored.

Example commands:
- `HOME id=1`
- `INSERT id=2 depth_mm=35 speed_mm_s=20`
- `REMOVE id=3`
- `STATUS id=4`
- `ABORT id=5`

#### 3.2 Responses

- One response line per command.
- Format:

  `RESULT id=<id> status=<OK|ERROR> [key=value ...]`

- `id` MUST be the same value sent in the request (if present).
- If `status=ERROR`, an error code MUST be present:
  - `error_code=<CODE>` and optionally `error_message="..."` (no spaces recommended initially).

Example responses:
- `RESULT id=1 status=OK state=READY`
- `RESULT id=2 status=OK state=INSERTED motion_time_ms=2300`
- `RESULT id=5 status=ERROR error_code=ILLEGAL_STATE state=ERROR`

#### 3.3 Asynchronous Events

- Event lines provide out-of-band notifications:

  `EVENT type=<TYPE> [key=value ...]`

- Common event types:
  - `STATE_CHANGED` – `old_state=<STATE> new_state=<STATE>`
  - `FAULT` – `error_code=<CODE> error_message=<string>` (e.g., `CARD_JAM`, `ESTOP_ASSERTED`)
  - `RESERVATION` – `owner=<string> action=<ACQUIRED|RELEASED|EXPIRED>`

Example events:
- `EVENT type=STATE_CHANGED old_state=IDLE new_state=INSERTING`
- `EVENT type=FAULT error_code=CARD_JAM error_message=Near_end_of_travel`

---

### 4. Command Set (Initial Version)

#### 4.1 `HOME`

**Purpose**: Move the mechanism to a known reference position (home).

Request:
- `HOME id=<int>`

Behavior:
- If already homed and in a safe state, device may either:
  - Do nothing and respond quickly.
  - Or re-home for additional assurance.
- During homing, state transitions:
  - `IDLE` → `HOMING` → `IDLE` (or `ERROR` on failure).

Response:
- On success:
  - `RESULT id=<id> status=OK state=IDLE`
- On failure (e.g., home sensor not found):
  - `RESULT id=<id> status=ERROR error_code=HOME_FAILED state=ERROR`

#### 4.2 `INSERT`

**Purpose**: Insert the card into the terminal slot to a given depth at a given speed.

Request:
- `INSERT id=<int> depth_mm=<int> [speed_mm_s=<int>]`

Parameters:
- `depth_mm`: nominal insertion depth in millimeters (required).
- `speed_mm_s`: nominal speed in mm/s (optional; default is device-configured).

Preconditions:
- Device must be in `IDLE` or `INSERTED`-compatible state.

Idempotency and safety:
- Repeated `INSERT` with the same parameters while in `INSERTED` SHOULD be treated as a no-op that returns `status=OK state=INSERTED` (unless unsafe).

Response:
- On success:
  - `RESULT id=<id> status=OK state=INSERTED motion_time_ms=<int>`
- On failure:
  - `RESULT id=<id> status=ERROR error_code=<CODE> state=ERROR|IDLE`

Potential error codes:
- `ILLEGAL_STATE` – command issued in an incompatible state.
- `MOTION_TIMEOUT` – motion did not complete in time.
- `CARD_JAM` – unexpected obstruction or stall detected.

#### 4.3 `REMOVE`

**Purpose**: Retract the card from the terminal slot.

Request:
- `REMOVE id=<int>`

Behavior:
- Retracts the carriage back to the safe/retracted position.

Idempotency:
- If the device is already in a state where the card is retracted (`IDLE` after homing), `REMOVE` SHOULD behave as a no-op and still return `status=OK state=IDLE`.

Response:
- On success:
  - `RESULT id=<id> status=OK state=IDLE motion_time_ms=<int>`
- On failure:
  - `RESULT id=<id> status=ERROR error_code=<CODE> state=ERROR|IDLE`

#### 4.4 `STATUS`

**Purpose**: Query current device state and last error.

Request:
- `STATUS id=<int>`

Response:
- On success:
  - `RESULT id=<id> status=OK state=<STATE> last_error_code=<CODE|NONE> last_error_message=<string_or_NONE>`

Example:
- `RESULT id=4 status=OK state=INSERTED last_error_code=NONE last_error_message=NONE`

#### 4.5 `ABORT`

**Purpose**: Immediately stop current motion and attempt to move to a safe condition.

Request:
- `ABORT id=<int>`

Behavior:
- Device stops motion as quickly as safely possible.
- Device may transition to `ERROR` or `IDLE` depending on implementation.

Response:
- On success:
  - `RESULT id=<id> status=OK state=<STATE_AFTER_ABORT>`
- On failure:
  - `RESULT id=<id> status=ERROR error_code=<CODE> state=ERROR`

Semantics:
- `ABORT` MUST:
  - Never drive the carriage further into the terminal than the position it had when the abort was processed.
  - Prefer retracting slightly or holding position depending on safety configuration.

#### 4.6 `RESET`

**Purpose**: Clear an `ERROR` or `ESTOP` condition (if safe) and return the device to a known initial state.

Request:
- `RESET id=<int>`

Behavior:
- If in `ERROR`, attempts a conservative homing or recovery sequence.
- If E-stop is asserted, `RESET` returns an error until E-stop is physically released.

Response:
- On success:
  - `RESULT id=<id> status=OK state=IDLE`
- On failure:
  - `RESULT id=<id> status=ERROR error_code=<CODE> state=ERROR`

---

### 5. Error Codes (Initial Set)

Error codes are short, uppercase identifiers:

- `ILLEGAL_STATE` – Command not allowed in current state.
- `HOME_FAILED` – Homing did not complete successfully.
- `MOTION_TIMEOUT` – Motion exceeded time budget.
- `CARD_JAM` – Jam or obstruction detected.
- `SENSOR_FAULT` – Sensor value inconsistent or out of expected range.
- `PROTOCOL_ERROR` – Malformed command or missing argument.
- `INTERNAL_ERROR` – Unspecified device-side error.
- `ESTOP_ASSERTED` – Emergency stop input active; motion commands rejected.
- `UNSAFE_CONFIGURATION` – Command parameters or configuration violate safety limits.

This set can be extended in future versions; new codes must be documented.

---

### 6. Versioning

The device reports protocol version via a special `STATUS` field:
- Example:
  - `protocol_version=1`
  - `min_compatible_protocol_version=1`
  - `features=EVENTS,RESET`

Future extensions to the protocol:
- MUST be backward compatible with commands and fields defined here.
- SHOULD avoid reusing codes or meanings.

Client behavior:
- On connect, the JVM client SHOULD:
  - Issue `STATUS` and read `protocol_version`, `min_compatible_protocol_version`, and `features`.
  - Refuse to operate (or operate in a degraded mode) if its required minimum version is greater than the device’s `protocol_version`.
  - Gracefully ignore unknown fields and unexpected additional key–value pairs.

The JVM client library tracks:
- Minimum required device protocol version.
- Graceful handling when connecting to a newer protocol with extra fields.

---

### 7. JVM API Design (Kotlin + Java)

#### 7.1 Core Types

Package suggestion:
- `com.yourcompany.emvphysicaltester`

Key classes (conceptual):

- `CardInserterClient`
  - Manages TCP connection and command/response lifecycle.
- `DeviceState`
  - Enum: `BOOTING`, `HOMING`, `IDLE`, `INSERTING`, `INSERTED`, `REMOVING`, `ERROR`.
- `InsertOptions`
  - Data class for `depthMm`, `speedMmPerSecond`, `timeout`.
- `InsertResult`, `RemoveResult`, `HomeResult`, `StatusResult`
  - Value types containing relevant fields and any returned telemetry.
- `DeviceException` and subclasses
  - `DeviceErrorException` (for `status=ERROR` responses).
  - `ProtocolException`, `TimeoutException`, `ConnectionException`, etc.

#### 7.2 Example Kotlin API

```kotlin
@JvmInline
value class DeviceId(val value: String)

@JvmInline
value class Millimeters(val value: Int)

@JvmInline
value class MillimetersPerSecond(val value: Int)

data class InsertOptions(
    val depth: Millimeters,
    val speed: MillimetersPerSecond? = null,
    val timeoutMillis: Long? = null
)

data class InsertResult(
    val state: DeviceState,
    val motionTimeMillis: Long?
)

interface CardInserterClient : AutoCloseable {

    val deviceId: DeviceId

    @Throws(DeviceException::class)
    fun home(timeoutMillis: Long? = null): DeviceState

    @Throws(DeviceException::class)
    fun insertCard(options: InsertOptions): InsertResult

    @Throws(DeviceException::class)
    fun removeCard(timeoutMillis: Long? = null): RemoveResult

    @Throws(DeviceException::class)
    fun status(): StatusResult

    @Throws(DeviceException::class)
    fun abort(): DeviceState

    @Throws(DeviceException::class)
    fun reset(timeoutMillis: Long? = null): DeviceState

    /**
     * Convenience: ensure card is inserted for the duration of [block],
     * then removed afterwards (even if [block] throws).
     */
    @Throws(DeviceException::class)
    fun <T> withCardInserted(
        options: InsertOptions,
        block: () -> T
    ): T
}
```

Notes:
- Expose static factory methods with `@JvmStatic` in a companion object or a separate `CardInserterClients` factory class, e.g.:

```kotlin
class CardInserterClients private constructor() {
    companion object {
        @JvmStatic
        fun connect(host: String, port: Int = 6000): CardInserterClient {
            // implementation
        }
    }
}
```

#### 7.3 Java Usage Example

```java
try (CardInserterClient client = CardInserterClients.connect("card-inserter-01.lab.local", 6000)) {
    client.home(10_000L);

    InsertOptions options = new InsertOptions(
        new Millimeters(35),          // depth
        null,                         // speed (use default)
        10_000L                       // timeoutMillis
    );

    client.withCardInserted(options, () -> {
        // Call EMV test code here
        runEmvL3Scenario();
        return null;
    });
}
```

---

### 8. Testing Strategy for Protocol and API

- **Unit tests**:
  - Command/response serialization and parsing.
  - Error handling and exception mapping.
- **Integration tests with simulator**:
  - Simulated device process implementing this protocol.
  - JVM client tests that exercise all commands, transitions, and error codes.
- **Hardware-in-the-loop tests**:
  - Periodic CI job that runs a subset of card motions against a real device.
  - Validates protocol compatibility and mechanical behavior end-to-end.

---

### 9. Simulation and Determinism Requirements

The simulator must:
- Implement the same protocol and state machine semantics as the real device.
- Be **deterministic by default**:
  - Given a fixed sequence of commands and seed, it must produce the same sequence of `RESULT` and `EVENT` lines.
- Provide **fault injection hooks**:
  - Ability to simulate `CARD_JAM`, `MOTION_TIMEOUT`, and `SENSOR_FAULT` after N operations or on demand.
- Allow configurable motion times so tests can:
  - Validate timeout behavior.
  - Run quickly in CI by shortening simulated motion durations.

The JVM client’s automated tests must:
- Run against both the simulator and at least one real device (for selected scenarios) to ensure behavioral parity.

This document defines the **external contracts** for the device. The simulator and real device must conform to this specification, and the JVM client must treat it as the single source of truth. 

