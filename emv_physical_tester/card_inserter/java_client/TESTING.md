# Card Inserter — Test Engineer Guide

This guide explains how to write JUnit 5 tests that physically insert and remove a card using the card inserter device.

## Prerequisites

- JDK 21 or later
- Gradle 9+ (or use the system Gradle)
- The card inserter device powered on and reachable over HTTP
- The device IP address or hostname

## Project structure

```
java_client/
├── build.gradle.kts
├── src/
│   ├── main/kotlin/com/emvphysicaltester/cardinserter/
│   │   ├── CardInserterClient.kt   # interface
│   │   ├── CardInserterClients.kt  # factory
│   │   ├── HttpCardInserterClient.kt
│   │   └── Types.kt               # DeviceState, InsertOptions, exceptions
│   └── test/kotlin/com/emvphysicaltester/cardinserter/
│       └── CardInserterTest.kt    # example tests
```

## Running the tests

Set `CARD_INSERTER_URL` to the device address and run Gradle:

```bash
CARD_INSERTER_URL=http://192.168.1.42 gradle test
```

Tests always run regardless of whether source files changed — Gradle's up-to-date check is disabled so that every `gradle test` invocation triggers real physical motion.

## Writing a test

### Kotlin

```kotlin
import com.emvphysicaltester.cardinserter.*
import org.junit.jupiter.api.*
import kotlin.test.assertEquals

class MyEmvTest {

    private lateinit var inserter: CardInserterClient

    @BeforeEach
    fun setUp() {
        val url = System.getenv("CARD_INSERTER_URL") ?: "http://localhost:8180"
        inserter = CardInserterClients.connect(url)
        inserter.home()   // move to known position before each test
    }

    @AfterEach
    fun tearDown() {
        runCatching { inserter.removeCard() }  // always retract, even if test failed
        inserter.close()
    }

    @Test
    fun `card inserted at full depth`() {
        val result = inserter.insertCard(InsertOptions(depthMm = 40))
        assertEquals(DeviceState.INSERTED, result.state)

        // run your EMV scenario here while card is inserted
    }
}
```

### Java

```java
import com.emvphysicaltester.cardinserter.*;
import org.junit.jupiter.api.*;
import static org.junit.jupiter.api.Assertions.*;

class MyEmvTest {

    private CardInserterClient inserter;

    @BeforeEach
    void setUp() {
        String url = System.getenv().getOrDefault("CARD_INSERTER_URL", "http://localhost:8180");
        inserter = CardInserterClients.connect(url);
        inserter.home();
    }

    @AfterEach
    void tearDown() {
        try { inserter.removeCard(); } catch (Exception ignored) {}
        inserter.close();
    }

    @Test
    void cardInsertedAtFullDepth() {
        InsertResult result = inserter.insertCard(new InsertOptions(40));
        assertEquals(DeviceState.INSERTED, result.getState());

        // run your EMV scenario here while card is inserted
    }
}
```

## API reference

### `CardInserterClients.connect`

```kotlin
CardInserterClients.connect(baseUrl: String, timeoutMillis: Long = 30_000L): CardInserterClient
```

Creates a client connected to the device at `baseUrl` (e.g. `http://192.168.1.42`). All HTTP requests will time out after `timeoutMillis` milliseconds.

### `CardInserterClient`

| Method | Description | Returns |
|--------|-------------|---------|
| `home()` | Move to the reference (home) position | `DeviceState` — `IDLE` on success |
| `insertCard(options)` | Insert the card to the specified depth | `InsertResult` — final state and motion time |
| `removeCard()` | Retract the card fully | `RemoveResult` — final state and motion time |
| `status()` | Query current device state without moving | `StatusResult` |
| `abort()` | Stop motion immediately | `DeviceState` |
| `close()` | Release the HTTP client | — |

### `InsertOptions`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `depthMm` | `Int` | required | How far to insert the card (mm). Device maximum is 40 mm. |
| `speedMmPerSecond` | `Int?` | device default | Insertion speed. Valid range: 5–80 mm/s. |
| `timeoutMillis` | `Long` | 30 000 | Client-side timeout for this request. |

### `DeviceState`

`BOOTING` → `HOMING` → `IDLE` ↔ `INSERTING` → `INSERTED` → `REMOVING` → `IDLE`

An `ERROR` state is entered on any fault. Call `home()` to recover.

### Exceptions

| Exception | When thrown |
|-----------|-------------|
| `DeviceErrorException` | Device returned `status: ERROR`. Check `.errorCode` and `.state`. |
| `ConnectionException` | Could not reach the device (network error, timeout). |
| `DeviceException` | Base class for both of the above. |

## Test structure recommendations

**Always home before each test** (`@BeforeEach`). This puts the device in a known position and clears any previous fault.

**Always remove in teardown** (`@AfterEach`). Wrap `removeCard()` in a try/catch so it runs even when the test fails, leaving the device ready for the next test.

**One physical action per test.** Keep each test focused on a single scenario. Chaining multiple insert/remove cycles in one test makes failures harder to diagnose.

**Check `motionTimeMillis`** in `InsertResult`/`RemoveResult` if your scenario has timing requirements — it reports how long the mechanical motion took.
