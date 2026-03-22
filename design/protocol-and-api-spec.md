## REST over HTTPS Protocol and JVM API Specification

### 1. Goals

- Provide a **simple, standards-based REST API over HTTPS** to control the card inserter.
- Ensure the API is **easy to debug** (via curl, Postman, or browser dev tools).
- Use **JSON** for request and response bodies so any tester can easily read or change them.
- Provide a **type-safe Kotlin API** that is also easy to use from Java-based tests.

The API is **request–response**: each action is an HTTP request; the response carries the result. Asynchronous events are available via Server-Sent Events (SSE).

---

### 2. Connection Model

- Transport: **HTTPS** (TLS 1.2 or later).
- Default port: **443** (configurable in firmware or lab gateway).
- Base URL: `https://<host>[:port]/api` (e.g. `https://card-inserter-01.lab.local/api`).
- Content type: **application/json** for request and response bodies.
- Character encoding: **UTF-8**.

Timeouts:

- Client-side **request timeout** is configurable in the JVM API (e.g. default 10 seconds).
- Device or gateway may close idle connections after a configurable idle period.

Asynchronous events:

- The device exposes **Server-Sent Events (SSE)** at `GET /api/events` to report:
  - State changes.
  - Faults (e.g., E-stop, sensor fault).
  - Reservation/lock changes.
- Clients that need real-time notifications subscribe to this stream; others may poll `GET /api/status` if preferred.

**TLS:** For lab use, TLS can be provided either (1) directly by the device (e.g. lightweight TLS stack on the MCU) or (2) by a lab reverse proxy/gateway in front of the device. The client always connects over HTTPS to a single base URL.

---

### 3. Message Format

#### 3.1 General Conventions

- **Requests:** JSON body with optional `id` for correlation; other fields are command-specific.
- **Responses:** JSON object with:
  - `id` – same as request (if present).
  - `status` – `"OK"` or `"ERROR"`.
  - For success: command-specific fields (e.g. `state`, `motion_time_ms`).
  - For error: `error_code`, and optionally `error_message`.

All endpoints return appropriate HTTP status codes:

- **200 OK** – Command succeeded (body contains result).
- **400 Bad Request** – Malformed request or invalid parameters.
- **409 Conflict** – Command not allowed in current state (maps to `ILLEGAL_STATE`).
- **500 Internal Server Error** – Device error (body includes `error_code`).
- **503 Service Unavailable** – E-stop asserted or device unavailable.

#### 3.2 Correlation ID

- Request bodies may include `"id": <number>` or `"id": "<string>"` for correlation.
- The response MUST echo the same `id` when present.

---

### 4. REST Resource Set (Initial Version)

#### 4.1 Home

**Purpose**: Move the mechanism to a known reference position (home).

- **Endpoint:** `POST /api/home`
- **Request body:**

```json
{ "id": 1 }
```

- **Behavior:** If already homed and in a safe state, device may do nothing and respond quickly or re-home. State transitions: `IDLE` → `HOMING` → `IDLE` (or `ERROR` on failure).
- **Response (200):**

```json
{ "id": 1, "status": "OK", "state": "IDLE" }
```

- **Response (error):**

```json
{ "id": 1, "status": "ERROR", "error_code": "HOME_FAILED", "state": "ERROR" }
```

#### 4.2 Insert

**Purpose**: Insert the card into the terminal slot to a given depth at a given speed.

- **Endpoint:** `POST /api/insert`
- **Request body:**

```json
{ "id": 2, "depth_mm": 35, "speed_mm_s": 20 }
```

- `depth_mm` (required): nominal insertion depth in millimeters.
- `speed_mm_s` (optional): nominal speed in mm/s; default is device-configured.
- **Preconditions:** Device must be in `IDLE` or compatible state.
- **Response (200):**

```json
{ "id": 2, "status": "OK", "state": "INSERTED", "motion_time_ms": 2300 }
```

- **Response (error):**

```json
{ "id": 2, "status": "ERROR", "error_code": "ILLEGAL_STATE", "state": "ERROR" }
```

- Potential error codes: `ILLEGAL_STATE`, `MOTION_TIMEOUT`, `CARD_JAM`.

#### 4.3 Remove

**Purpose**: Retract the card from the terminal slot.

- **Endpoint:** `POST /api/remove`
- **Request body:**

```json
{ "id": 3 }
```

- **Response (200):**

```json
{ "id": 3, "status": "OK", "state": "IDLE", "motion_time_ms": 1800 }
```

- **Response (error):** Same pattern with `error_code` and `state`.

#### 4.4 Status

**Purpose**: Query current device state and last error.

- **Endpoint:** `GET /api/status`
- **Query parameters (optional):** `id` for correlation in response.
- **Response (200):**

```json
{
  "id": 4,
  "status": "OK",
  "state": "INSERTED",
  "last_error_code": "NONE",
  "last_error_message": "NONE",
  "protocol_version": 1,
  "min_compatible_protocol_version": 1,
  "features": ["EVENTS", "RESET"]
}
```

#### 4.5 Abort

**Purpose**: Immediately stop current motion and attempt to move to a safe condition.

- **Endpoint:** `POST /api/abort`
- **Request body:**

```json
{ "id": 5 }
```

- **Response (200):**

```json
{ "id": 5, "status": "OK", "state": "IDLE" }
```

- **Semantics:** ABORT MUST never drive the carriage further into the terminal than the position at abort; prefer retracting or holding per safety configuration.

#### 4.6 Reset

**Purpose**: Clear an `ERROR` or `ESTOP` condition (if safe) and return to a known initial state.

- **Endpoint:** `POST /api/reset`
- **Request body:**

```json
{ "id": 6 }
```

- **Response (200):** `{ "id": 6, "status": "OK", "state": "IDLE" }`
- **Response (error):** If E-stop still asserted, returns error until E-stop is physically released.

#### 4.7 Asynchronous Events (SSE)

- **Endpoint:** `GET /api/events`
- **Accept:** `text/event-stream`
- **Behavior:** Server sends Server-Sent Events. Each event is a JSON object (one per line after `data: `).

Event types:

- **STATE_CHANGED** – `{ "type": "STATE_CHANGED", "old_state": "IDLE", "new_state": "INSERTING" }`
- **FAULT** – `{ "type": "FAULT", "error_code": "CARD_JAM", "error_message": "Near_end_of_travel" }`
- **RESERVATION** – `{ "type": "RESERVATION", "owner": "<string>", "action": "ACQUIRED" | "RELEASED" | "EXPIRED" }`

**Device reservation:** Only one logical test session should control a device at a time. Implementations may use (1) **First client wins** – first successful command holder has logical ownership until disconnect, or (2) **Explicit reserve/release** – optional `POST /api/reserve` and `POST /api/release` with `owner` and optional `lease_sec`; device emits RESERVATION events on change. The JVM client or lab scheduler must enforce exclusive use.

---

### 5. Error Codes (Initial Set)

Error codes are short, uppercase string identifiers:

- `ILLEGAL_STATE` – Command not allowed in current state.
- `HOME_FAILED` – Homing did not complete successfully.
- `MOTION_TIMEOUT` – Motion exceeded time budget.
- `CARD_JAM` – Jam or obstruction detected.
- `SENSOR_FAULT` – Sensor value inconsistent or out of expected range.
- `PROTOCOL_ERROR` – Malformed request or missing required field.
- `INTERNAL_ERROR` – Unspecified device-side error.
- `ESTOP_ASSERTED` – Emergency stop input active; motion commands rejected.
- `UNSAFE_CONFIGURATION` – Command parameters or configuration violate safety limits.

This set can be extended in future versions; new codes must be documented.

---

### 6. Versioning

The device reports protocol version in the **status** response:

- `protocol_version` – current API version.
- `min_compatible_protocol_version` – minimum client version the device supports.
- `features` – optional array of feature flags (e.g. `EVENTS`, `RESET`).

Future extensions:

- MUST be backward compatible with resources and fields defined here.
- SHOULD avoid reusing codes or meanings.

Client behavior:

- The JVM client SHOULD call `GET /api/status` (or include version in first request) and check `protocol_version` and `min_compatible_protocol_version`.
- Refuse to operate (or operate in degraded mode) if the client’s required minimum version is greater than the device’s `protocol_version`.
- Gracefully ignore unknown JSON fields.

---

### 7. JVM API Design (Kotlin + Java)

#### 7.1 Core Types

Package suggestion:

- `com.yourcompany.emvphysicaltester`

Key classes (conceptual):

- `CardInserterClient`
  - Uses an HTTP client and base URL; sends REST requests and parses JSON responses.
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
        fun connect(baseUrl: String): CardInserterClient {
            // e.g. baseUrl = "https://card-inserter-01.lab.local"
            // implementation uses HttpClient, builds URLs like baseUrl + "/api/home", etc.
        }
    }
}
```

#### 7.3 Java Usage Example

```java
try (CardInserterClient client = CardInserterClients.connect("https://card-inserter-01.lab.local")) {
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

- **Unit tests:**
  - Request/response JSON serialization and parsing.
  - Error handling and exception mapping from HTTP status and body.
- **Integration tests with simulator:**
  - Simulated device (or mock server) implementing this REST API.
  - JVM client tests that exercise all endpoints, state transitions, and error codes.
- **Hardware-in-the-loop tests:**
  - Periodic CI job that runs a subset of card motions against a real device over HTTPS.
  - Validates API compatibility and mechanical behavior end-to-end.

---

### 9. Simulation and Determinism Requirements

The simulator must:

- Implement the same REST API and state machine semantics as the real device.
- Be **deterministic by default**:
  - Given a fixed sequence of requests and seed, produce the same sequence of responses and SSE events.
- Provide **fault injection hooks**:
  - Ability to simulate `CARD_JAM`, `MOTION_TIMEOUT`, and `SENSOR_FAULT` after N operations or on demand.
- Allow configurable motion times so tests can validate timeout behavior and run quickly in CI.

The JVM client’s automated tests must:

- Run against both the simulator and at least one real device (for selected scenarios) to ensure behavioral parity.

This document defines the **external contracts** for the device. The simulator and real device must conform to this specification, and the JVM client must treat it as the single source of truth.
