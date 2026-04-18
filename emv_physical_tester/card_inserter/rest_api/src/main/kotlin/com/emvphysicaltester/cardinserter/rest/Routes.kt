package com.emvphysicaltester.cardinserter.rest

import com.emvphysicaltester.cardinserter.engine.*
import com.google.gson.Gson
import com.google.gson.JsonElement
import com.google.gson.JsonObject
import com.google.gson.JsonPrimitive
import com.sun.net.httpserver.HttpExchange
import com.sun.net.httpserver.HttpServer
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

private val gson = Gson()

private val METHOD_NOT_ALLOWED = """{"status":"ERROR","state":"ERROR","error_code":"PROTOCOL_ERROR","error_message":"Method not allowed"}"""

fun HttpServer.registerRoutes(engine: DeviceEngine) {

    createContext("/api/home") { ex ->
        if (ex.requestMethod != "POST") { ex.sendJson(405, METHOD_NOT_ALLOWED); return@createContext }
        val req = runCatching { gson.fromJson(ex.readBody().ifEmpty { "{}" }, HomeRequest::class.java) }
            .getOrDefault(HomeRequest())
        ex.respondResult(engine.home(), req?.id)
    }

    createContext("/api/insert") { ex ->
        if (ex.requestMethod != "POST") { ex.sendJson(405, METHOD_NOT_ALLOWED); return@createContext }
        val body = ex.readBody()
        val req = try {
            val r = gson.fromJson(body.ifEmpty { "{}" }, InsertRequest::class.java)
            requireNotNull(r?.depth_mm) { "depth_mm is required" }
            r
        } catch (e: Exception) {
            ex.sendJson(400, gson.toJson(ActionResponse(
                status = "ERROR", state = engine.currentState.name,
                error_code = ErrCode.PROTOCOL_ERROR.name, error_message = e.message,
            )))
            return@createContext
        }
        ex.respondResult(engine.insert(req.depth_mm!!, req.speed_mm_s), req.id)
    }

    createContext("/api/remove") { ex ->
        if (ex.requestMethod != "POST") { ex.sendJson(405, METHOD_NOT_ALLOWED); return@createContext }
        val req = runCatching { gson.fromJson(ex.readBody().ifEmpty { "{}" }, RemoveRequest::class.java) }
            .getOrDefault(RemoveRequest())
        ex.respondResult(engine.remove(), req?.id)
    }

    createContext("/api/status") { ex ->
        if (ex.requestMethod != "GET") { ex.sendJson(405, METHOD_NOT_ALLOWED); return@createContext }
        val idParam = ex.requestURI.query?.split("&")
            ?.firstOrNull { it.startsWith("id=") }?.removePrefix("id=")
        val idElement: JsonElement? = idParam?.let {
            it.toLongOrNull()?.let { n -> JsonPrimitive(n) } ?: JsonPrimitive(it)
        }
        val s = engine.getStatus()
        ex.sendJson(200, gson.toJson(StatusResponse(
            id = idElement, status = "OK", state = s.state.name,
            last_error_code = s.lastErrorCode.name, last_error_message = s.lastErrorMessage,
        )))
    }

    createContext("/api/abort") { ex ->
        if (ex.requestMethod != "POST") { ex.sendJson(405, METHOD_NOT_ALLOWED); return@createContext }
        val req = runCatching { gson.fromJson(ex.readBody().ifEmpty { "{}" }, AbortRequest::class.java) }
            .getOrDefault(AbortRequest())
        ex.respondResult(engine.abort(), req?.id)
    }

    createContext("/api/reset") { ex ->
        if (ex.requestMethod != "POST") { ex.sendJson(405, METHOD_NOT_ALLOWED); return@createContext }
        val req = runCatching { gson.fromJson(ex.readBody().ifEmpty { "{}" }, ResetRequest::class.java) }
            .getOrDefault(ResetRequest())
        ex.respondResult(engine.reset(), req?.id)
    }

    createContext("/api/events") { ex ->
        if (ex.requestMethod != "GET") { ex.sendJson(405, METHOD_NOT_ALLOWED); return@createContext }
        ex.responseHeaders.add("Content-Type", "text/event-stream")
        ex.responseHeaders.add("Cache-Control", "no-cache")
        ex.responseHeaders.add("Connection", "keep-alive")
        ex.sendResponseHeaders(200, 0)
        val queue = LinkedBlockingQueue<SseEvent>()
        val listener: (SseEvent) -> Unit = { queue.offer(it) }
        engine.addEventListener(listener)
        val out = ex.responseBody
        try {
            while (!Thread.currentThread().isInterrupted) {
                val event = queue.poll(30, TimeUnit.SECONDS)
                val line = if (event != null) "data: ${event.toJsonString()}\n\n"
                           else ": heartbeat\n\n"
                out.write(line.toByteArray())
                out.flush()
            }
        } catch (_: Exception) {
        } finally {
            engine.removeEventListener(listener)
            runCatching { out.close() }
        }
    }
}

private fun SseEvent.toJsonString(): String {
    val obj = gson.toJsonTree(this).asJsonObject
    obj.addProperty("type", type)
    return obj.toString()
}

private fun HttpExchange.readBody(): String = requestBody.readBytes().decodeToString()

internal fun HttpExchange.sendJson(statusCode: Int, body: String) {
    val bytes = body.toByteArray(Charsets.UTF_8)
    responseHeaders.add("Content-Type", "application/json; charset=utf-8")
    sendResponseHeaders(statusCode, bytes.size.toLong())
    responseBody.use { it.write(bytes) }
}

private fun ErrCode.toHttpStatus(): Int = when (this) {
    ErrCode.NONE -> 200
    ErrCode.PROTOCOL_ERROR, ErrCode.UNSAFE_CONFIGURATION -> 400
    ErrCode.ILLEGAL_STATE -> 409
    ErrCode.ESTOP_ASSERTED -> 503
    else -> 500
}

private fun EngineResult.toActionResponse(id: JsonElement? = null) = ActionResponse(
    id = id,
    status = if (isSuccess) "OK" else "ERROR",
    state = state.name,
    motion_time_ms = motionTimeMs,
    error_code = if (!isSuccess) errorCode.name else null,
    error_message = if (!isSuccess && errorMessage.isNotEmpty()) errorMessage else null,
)

private fun HttpExchange.respondResult(result: EngineResult, id: JsonElement? = null) =
    sendJson(result.errorCode.toHttpStatus(), gson.toJson(result.toActionResponse(id)))
