package com.emvphysicaltester.cardinserter

import com.emvphysicaltester.cardinserter.engine.*
import com.emvphysicaltester.cardinserter.rest.*
import com.emvphysicaltester.cardinserter.client.*
import com.google.gson.Gson
import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest
import java.net.http.HttpResponse

// ── Helpers ──────────────────────────────────────────────────────────────────

private fun instantEngine(autoHome: Boolean = true) = DeviceEngine(
    config = DeviceConfig(homingTimeMs = 0),
    delayFn = { _ -> },
).also { if (autoHome) it.home() }

private fun withServer(
    engine: DeviceEngine = instantEngine(),
    block: (baseUrl: String, engine: DeviceEngine) -> Unit,
) {
    val server = createServer(engine, port = 0)
    server.start()
    try {
        block("http://localhost:${server.address.port}", engine)
    } finally {
        server.stop(0)
    }
}

private val testHttpClient: HttpClient = HttpClient.newHttpClient()
private val gson = Gson()

private fun postJson(url: String, body: String = "{}"): Pair<Int, String> {
    val resp = testHttpClient.send(
        HttpRequest.newBuilder().uri(URI.create(url))
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(body)).build(),
        HttpResponse.BodyHandlers.ofString(),
    )
    return resp.statusCode() to resp.body()
}

private fun getJson(url: String): Pair<Int, String> {
    val resp = testHttpClient.send(
        HttpRequest.newBuilder().uri(URI.create(url)).GET().build(),
        HttpResponse.BodyHandlers.ofString(),
    )
    return resp.statusCode() to resp.body()
}

// ── Runner ───────────────────────────────────────────────────────────────────

class Suite(val name: String) {
    val tests = mutableListOf<Pair<String, () -> Unit>>()
    fun test(name: String, block: () -> Unit) { tests += name to block }
}

fun main() {
    val suites = listOf(
        engineSuite(),
        routesSuite(),
        clientSuite(),
    )

    var passed = 0
    var failed = 0

    for (suite in suites) {
        println("\n[${suite.name}]")
        for ((name, block) in suite.tests) {
            try {
                block()
                println("  PASS  $name")
                passed++
            } catch (e: AssertionError) {
                println("  FAIL  $name")
                println("        ${e.message}")
                failed++
            } catch (e: Exception) {
                println("  ERROR $name")
                e.printStackTrace(System.out)
                failed++
            }
        }
    }

    println("\n════════════════════════════════════════")
    println("$passed passed, $failed failed")
    if (failed > 0) {
        System.err.println("TESTS FAILED")
        System.exit(1)
    }
}

// ── Engine tests ──────────────────────────────────────────────────────────────

private fun engineSuite() = Suite("DeviceEngine").apply {

    test("initial state is BOOTING") {
        assertEquals(DeviceState.BOOTING, instantEngine(autoHome = false).currentState)
    }

    test("home transitions BOOTING to IDLE") {
        val e = instantEngine(autoHome = false)
        val r = e.home()
        assertEquals(DeviceState.IDLE, r.state)
        assertTrue(r.isSuccess)
    }

    test("home is idempotent from IDLE") {
        val e = instantEngine()
        assertEquals(DeviceState.IDLE, e.home().state)
    }

    test("insert from IDLE returns INSERTED") {
        val e = instantEngine()
        val r = e.insert(35.0)
        assertEquals(DeviceState.INSERTED, r.state)
        assertTrue(r.isSuccess)
    }

    test("remove from INSERTED returns IDLE") {
        val e = instantEngine()
        e.insert(35.0)
        assertEquals(DeviceState.IDLE, e.remove().state)
    }

    test("insert when not IDLE returns ILLEGAL_STATE") {
        assertEquals(ErrCode.ILLEGAL_STATE, instantEngine(autoHome = false).insert(35.0).errorCode)
    }

    test("remove when not INSERTED returns ILLEGAL_STATE") {
        assertEquals(ErrCode.ILLEGAL_STATE, instantEngine().remove().errorCode)
    }

    test("insert with depth too large returns UNSAFE_CONFIGURATION") {
        val e = instantEngine()
        assertEquals(ErrCode.UNSAFE_CONFIGURATION, e.insert(9999.0).errorCode)
        assertEquals(DeviceState.IDLE, e.currentState)
    }

    test("insert with depth too small returns UNSAFE_CONFIGURATION") {
        assertEquals(ErrCode.UNSAFE_CONFIGURATION, instantEngine().insert(0.01).errorCode)
    }

    test("insert with speed too high returns UNSAFE_CONFIGURATION") {
        assertEquals(ErrCode.UNSAFE_CONFIGURATION, instantEngine().insert(35.0, 99999.0).errorCode)
    }

    test("abort during insert stops motion returning IDLE") {
        val slow = DeviceEngine(config = DeviceConfig(homingTimeMs = 0), delayFn = { ms -> Thread.sleep(ms) })
        slow.home()
        val future = java.util.concurrent.CompletableFuture.supplyAsync { slow.insert(35.0, 0.5) }
        Thread.sleep(30)
        slow.abort()
        val result = future.get(3, java.util.concurrent.TimeUnit.SECONDS)
        assertEquals(DeviceState.IDLE, slow.currentState)
        assertEquals(DeviceState.IDLE, result.state)
    }

    test("abort when idle is a no-op") {
        val e = instantEngine()
        assertEquals(DeviceState.IDLE, e.abort().state)
    }

    test("reset from IDLE returns IDLE") {
        assertEquals(DeviceState.IDLE, instantEngine().reset().state)
    }

    test("reset from ERROR re-homes to IDLE") {
        val e = instantEngine()
        e.assertEStop(); e.releaseEStop()
        assertEquals(DeviceState.ERROR, e.currentState)
        assertEquals(DeviceState.IDLE, e.reset().state)
    }

    test("reset from BOOTING returns ILLEGAL_STATE") {
        assertEquals(ErrCode.ILLEGAL_STATE, instantEngine(autoHome = false).reset().errorCode)
    }

    test("ESTOP blocks home") {
        val e = instantEngine(); e.assertEStop()
        assertEquals(ErrCode.ESTOP_ASSERTED, e.home().errorCode)
    }

    test("ESTOP blocks insert") {
        val e = instantEngine(); e.assertEStop()
        assertEquals(ErrCode.ESTOP_ASSERTED, e.insert(35.0).errorCode)
    }

    test("ESTOP blocks remove") {
        val e = instantEngine(); e.insert(35.0); e.assertEStop()
        assertEquals(ErrCode.ESTOP_ASSERTED, e.remove().errorCode)
    }

    test("ESTOP blocks reset") {
        val e = instantEngine(); e.assertEStop()
        assertEquals(ErrCode.ESTOP_ASSERTED, e.reset().errorCode)
    }

    test("releaseEStop transitions to ERROR") {
        val e = instantEngine(); e.assertEStop(); e.releaseEStop()
        assertEquals(DeviceState.ERROR, e.currentState)
    }

    test("SSE events fired on home") {
        val events = mutableListOf<SseEvent>()
        val e = instantEngine(autoHome = false)
        e.addEventListener { events.add(it) }
        e.home()
        val states = events.filterIsInstance<SseEvent.StateChanged>().map { it.new_state }
        assertTrue(states.contains(DeviceState.HOMING.name))
        assertTrue(states.contains(DeviceState.IDLE.name))
    }

    test("SSE events fired on insert and remove") {
        val e = instantEngine()
        val events = mutableListOf<SseEvent>()
        e.addEventListener { events.add(it) }
        e.insert(35.0); e.remove()
        val states = events.filterIsInstance<SseEvent.StateChanged>().map { it.new_state }
        assertTrue(states.containsAll(listOf("INSERTING", "INSERTED", "REMOVING", "IDLE")))
    }

    test("SSE fault event on ESTOP") {
        val events = mutableListOf<SseEvent>()
        val e = instantEngine()
        e.addEventListener { events.add(it) }
        e.assertEStop()
        assertTrue(events.filterIsInstance<SseEvent.Fault>().any { it.error_code == ErrCode.ESTOP_ASSERTED.name })
    }

    test("listener removed stops receiving events") {
        val events = mutableListOf<SseEvent>()
        val listener: (SseEvent) -> Unit = { events.add(it) }
        val e = instantEngine(autoHome = false)
        e.addEventListener(listener); e.home()
        val countAfterHome = events.size
        e.removeEventListener(listener); e.insert(35.0)
        assertEquals(countAfterHome, events.size)
    }

    test("getStatus reflects state") {
        val e = instantEngine(autoHome = false)
        assertEquals(DeviceState.BOOTING, e.getStatus().state)
        e.home()
        assertEquals(DeviceState.IDLE, e.getStatus().state)
        assertEquals(ErrCode.NONE, e.getStatus().lastErrorCode)
    }

    test("full insert-remove cycle") {
        val e = instantEngine()
        e.insert(20.0, 10.0)
        assertEquals(DeviceState.INSERTED, e.currentState)
        e.remove()
        assertEquals(DeviceState.IDLE, e.currentState)
    }
}

// ── Routes tests ──────────────────────────────────────────────────────────────

private fun routesSuite() = Suite("REST Routes").apply {

    test("POST /api/home returns 200 IDLE") {
        withServer { url, _ ->
            val (status, body) = postJson("$url/api/home")
            assertEquals(200, status)
            val r = gson.fromJson(body, ActionResponse::class.java)
            assertEquals("OK", r.status); assertEquals("IDLE", r.state)
        }
    }

    test("POST /api/home echoes correlation id") {
        withServer { url, _ ->
            val (_, body) = postJson("$url/api/home", """{"id":42}""")
            val r = gson.fromJson(body, ActionResponse::class.java)
            assertEquals(42L, r.id?.asLong)
        }
    }

    test("POST /api/insert returns 200 INSERTED with motion_time_ms") {
        withServer { url, _ ->
            val (status, body) = postJson("$url/api/insert", """{"depth_mm":35}""")
            assertEquals(200, status)
            val r = gson.fromJson(body, ActionResponse::class.java)
            assertEquals("OK", r.status); assertEquals("INSERTED", r.state)
            assertNotNull(r.motion_time_ms)
        }
    }

    test("POST /api/insert invalid depth returns 400 UNSAFE_CONFIGURATION") {
        withServer { url, _ ->
            val (status, body) = postJson("$url/api/insert", """{"depth_mm":9999}""")
            assertEquals(400, status)
            assertEquals("UNSAFE_CONFIGURATION", gson.fromJson(body, ActionResponse::class.java).error_code)
        }
    }

    test("POST /api/insert missing depth_mm returns 400 PROTOCOL_ERROR") {
        withServer { url, _ ->
            val (status, body) = postJson("$url/api/insert", "{}")
            assertEquals(400, status)
            assertEquals("PROTOCOL_ERROR", gson.fromJson(body, ActionResponse::class.java).error_code)
        }
    }

    test("POST /api/insert in wrong state returns 409 ILLEGAL_STATE") {
        withServer(engine = instantEngine(autoHome = false)) { url, _ ->
            val (status, body) = postJson("$url/api/insert", """{"depth_mm":35}""")
            assertEquals(409, status)
            assertEquals("ILLEGAL_STATE", gson.fromJson(body, ActionResponse::class.java).error_code)
        }
    }

    test("POST /api/insert with speed returns INSERTED") {
        withServer { url, _ ->
            val (status, body) = postJson("$url/api/insert", """{"depth_mm":30,"speed_mm_s":50}""")
            assertEquals(200, status)
            assertEquals("INSERTED", gson.fromJson(body, ActionResponse::class.java).state)
        }
    }

    test("POST /api/remove returns 200 IDLE with motion_time_ms") {
        withServer(engine = instantEngine().also { it.insert(35.0) }) { url, _ ->
            val (status, body) = postJson("$url/api/remove")
            assertEquals(200, status)
            val r = gson.fromJson(body, ActionResponse::class.java)
            assertEquals("IDLE", r.state); assertNotNull(r.motion_time_ms)
        }
    }

    test("POST /api/remove not INSERTED returns 409") {
        withServer { url, _ ->
            val (status, _) = postJson("$url/api/remove")
            assertEquals(409, status)
        }
    }

    test("GET /api/status returns 200 full status object") {
        withServer { url, _ ->
            val (status, body) = getJson("$url/api/status")
            assertEquals(200, status)
            val r = gson.fromJson(body, StatusResponse::class.java)
            assertEquals("OK", r.status); assertEquals("IDLE", r.state)
            assertEquals("NONE", r.last_error_code)
            assertEquals(1, r.protocol_version)
            assertTrue(r.features.contains("EVENTS"))
            assertTrue(r.features.contains("RESET"))
        }
    }

    test("GET /api/status echoes numeric id query param") {
        withServer { url, _ ->
            val (_, body) = getJson("$url/api/status?id=7")
            val r = gson.fromJson(body, StatusResponse::class.java)
            assertEquals(7L, r.id?.asLong)
        }
    }

    test("GET /api/status echoes string id query param") {
        withServer { url, _ ->
            val (_, body) = getJson("$url/api/status?id=req-abc")
            val r = gson.fromJson(body, StatusResponse::class.java)
            assertEquals("req-abc", r.id?.asString)
        }
    }

    test("POST /api/abort when idle returns 200 IDLE") {
        withServer { url, _ ->
            val (status, body) = postJson("$url/api/abort")
            assertEquals(200, status)
            assertEquals("IDLE", gson.fromJson(body, ActionResponse::class.java).state)
        }
    }

    test("POST /api/reset from IDLE returns 200 IDLE") {
        withServer { url, _ ->
            val (status, body) = postJson("$url/api/reset")
            assertEquals(200, status)
            assertEquals("IDLE", gson.fromJson(body, ActionResponse::class.java).state)
        }
    }

    test("POST /api/reset from BOOTING returns 409") {
        withServer(engine = instantEngine(autoHome = false)) { url, _ ->
            val (status, _) = postJson("$url/api/reset")
            assertEquals(409, status)
        }
    }

    test("home with ESTOP returns 503") {
        withServer(engine = instantEngine().also { it.assertEStop() }) { url, _ ->
            val (status, body) = postJson("$url/api/home")
            assertEquals(503, status)
            assertEquals("ESTOP_ASSERTED", gson.fromJson(body, ActionResponse::class.java).error_code)
        }
    }

    test("insert with ESTOP returns 503") {
        withServer(engine = instantEngine().also { it.assertEStop() }) { url, _ ->
            val (status, _) = postJson("$url/api/insert", """{"depth_mm":35}""")
            assertEquals(503, status)
        }
    }

    test("successful response has no error fields") {
        withServer { url, _ ->
            val r = gson.fromJson(postJson("$url/api/home").second, ActionResponse::class.java)
            assertNull(r.error_code); assertNull(r.error_message)
        }
    }

    test("error response includes error_code and error_message") {
        withServer(engine = instantEngine(autoHome = false)) { url, _ ->
            val r = gson.fromJson(postJson("$url/api/insert", """{"depth_mm":35}""").second, ActionResponse::class.java)
            assertNotNull(r.error_code); assertNotNull(r.error_message)
        }
    }
}

// ── Client tests ──────────────────────────────────────────────────────────────

private fun clientSuite() = Suite("CardInserterClient").apply {

    fun withClient(engine: DeviceEngine = instantEngine(), block: (CardInserterClient) -> Unit) {
        withServer(engine) { url, _ ->
            HttpCardInserterClient(url).use { block(it) }
        }
    }

    test("home returns HomeResult IDLE") {
        withClient { assertEquals(DeviceState.IDLE, it.home().state) }
    }

    test("home from BOOTING transitions to IDLE") {
        withClient(instantEngine(autoHome = false)) { assertEquals(DeviceState.IDLE, it.home().state) }
    }

    test("insertCard returns INSERTED with motionTimeMillis") {
        withClient {
            val r = it.insertCard(InsertOptions(Millimeters(35.0)))
            assertEquals(DeviceState.INSERTED, r.state); assertNotNull(r.motionTimeMillis)
        }
    }

    test("insertCard with explicit speed returns INSERTED") {
        withClient {
            assertEquals(DeviceState.INSERTED, it.insertCard(InsertOptions(Millimeters(20.0), MillimetersPerSecond(50.0))).state)
        }
    }

    test("insertCard throws DeviceErrorException when not IDLE") {
        withClient(instantEngine(autoHome = false)) {
            val ex = assertThrows(DeviceException.DeviceErrorException::class.java) {
                it.insertCard(InsertOptions(Millimeters(35.0)))
            }
            assertEquals("ILLEGAL_STATE", ex.errorCode)
        }
    }

    test("insertCard throws on unsafe depth") {
        withClient {
            val ex = assertThrows(DeviceException.DeviceErrorException::class.java) {
                it.insertCard(InsertOptions(Millimeters(9999.0)))
            }
            assertEquals("UNSAFE_CONFIGURATION", ex.errorCode)
        }
    }

    test("removeCard returns IDLE with motionTimeMillis") {
        withClient {
            it.insertCard(InsertOptions(Millimeters(35.0)))
            val r = it.removeCard()
            assertEquals(DeviceState.IDLE, r.state); assertNotNull(r.motionTimeMillis)
        }
    }

    test("removeCard throws ILLEGAL_STATE when not INSERTED") {
        withClient {
            val ex = assertThrows(DeviceException.DeviceErrorException::class.java) { it.removeCard() }
            assertEquals("ILLEGAL_STATE", ex.errorCode)
        }
    }

    test("status returns StatusResult with correct fields") {
        withClient {
            val s = it.status()
            assertEquals(DeviceState.IDLE, s.state)
            assertEquals("NONE", s.lastErrorCode)
            assertEquals(1, s.protocolVersion)
            assertTrue(s.features.contains("EVENTS"))
            assertTrue(s.features.contains("RESET"))
        }
    }

    test("status reflects INSERTED after insertCard") {
        withClient {
            it.insertCard(InsertOptions(Millimeters(35.0)))
            assertEquals(DeviceState.INSERTED, it.status().state)
        }
    }

    test("abort when idle returns IDLE") {
        withClient { assertEquals(DeviceState.IDLE, it.abort()) }
    }

    test("reset from IDLE returns IDLE") {
        withClient { assertEquals(DeviceState.IDLE, it.reset()) }
    }

    test("reset throws ILLEGAL_STATE from BOOTING") {
        withClient(instantEngine(autoHome = false)) {
            val ex = assertThrows(DeviceException.DeviceErrorException::class.java) { it.reset() }
            assertEquals("ILLEGAL_STATE", ex.errorCode)
        }
    }

    test("withCardInserted executes block while INSERTED then returns to IDLE") {
        val engine = instantEngine()
        withClient(engine) { client ->
            var executed = false
            client.withCardInserted(InsertOptions(Millimeters(35.0))) {
                executed = true
                assertEquals(DeviceState.INSERTED, engine.currentState)
            }
            assertTrue(executed)
            assertEquals(DeviceState.IDLE, engine.currentState)
        }
    }

    test("withCardInserted removes card even if block throws") {
        val engine = instantEngine()
        withClient(engine) { client ->
            try {
                client.withCardInserted(InsertOptions(Millimeters(35.0))) {
                    throw RuntimeException("test error")
                }
            } catch (_: RuntimeException) {}
            assertEquals(DeviceState.IDLE, engine.currentState)
        }
    }

    test("withCardInserted returns block result") {
        withClient {
            val result = it.withCardInserted(InsertOptions(Millimeters(35.0))) { 42 }
            assertEquals(42, result)
        }
    }

    test("full home-insert-status-remove workflow") {
        val engine = instantEngine(autoHome = false)
        withClient(engine) { client ->
            client.home()
            client.insertCard(InsertOptions(Millimeters(30.0), MillimetersPerSecond(50.0)))
            assertEquals(DeviceState.INSERTED, client.status().state)
            client.removeCard()
            assertEquals(DeviceState.IDLE, client.status().state)
        }
    }
}
