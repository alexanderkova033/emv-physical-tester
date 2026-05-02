package com.emvphysicaltester.cardinserter.client

import com.emvphysicaltester.cardinserter.engine.DeviceState
import com.emvphysicaltester.cardinserter.rest.*
import com.google.gson.Gson
import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest
import java.net.http.HttpResponse

private val gson = Gson()

class HttpCardInserterClient(
    private val baseUrl: String,
    private val httpClient: HttpClient = HttpClient.newHttpClient(),
) : CardInserterClient {

    private fun post(path: String, body: String = "{}"): String =
        httpClient.send(
            HttpRequest.newBuilder()
                .uri(URI.create("$baseUrl/api$path"))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(body))
                .build(),
            HttpResponse.BodyHandlers.ofString(),
        ).body()

    private fun get(path: String, query: String = ""): String {
        val uri = if (query.isEmpty()) "$baseUrl/api$path" else "$baseUrl/api$path?$query"
        return httpClient.send(
            HttpRequest.newBuilder().uri(URI.create(uri)).GET().build(),
            HttpResponse.BodyHandlers.ofString(),
        ).body()
    }

    override fun home(timeoutMillis: Long?): HomeResult {
        val body = gson.fromJson(post("/home"), ActionResponse::class.java)
        body.throwIfError()
        return HomeResult(DeviceState.valueOf(body.state))
    }

    override fun insertCard(options: InsertOptions): InsertResult {
        val req = InsertRequest(depth_mm = options.depth.value, speed_mm_s = options.speed?.value)
        val body = gson.fromJson(post("/insert", gson.toJson(req)), ActionResponse::class.java)
        body.throwIfError()
        return InsertResult(DeviceState.valueOf(body.state), body.motion_time_ms)
    }

    override fun removeCard(timeoutMillis: Long?): RemoveResult {
        val body = gson.fromJson(post("/remove"), ActionResponse::class.java)
        body.throwIfError()
        return RemoveResult(DeviceState.valueOf(body.state), body.motion_time_ms)
    }

    override fun status(): StatusResult {
        val body = gson.fromJson(get("/status"), StatusResponse::class.java)
        return StatusResult(
            state = DeviceState.valueOf(body.state),
            lastErrorCode = body.last_error_code,
            lastErrorMessage = body.last_error_message,
            protocolVersion = body.protocol_version,
            minCompatibleProtocolVersion = body.min_compatible_protocol_version,
            features = body.features,
        )
    }

    override fun abort(): DeviceState {
        val body = gson.fromJson(post("/abort"), ActionResponse::class.java)
        return DeviceState.valueOf(body.state)
    }

    override fun reset(timeoutMillis: Long?): DeviceState {
        val body = gson.fromJson(post("/reset"), ActionResponse::class.java)
        body.throwIfError()
        return DeviceState.valueOf(body.state)
    }

    override fun <T> withCardInserted(options: InsertOptions, block: () -> T): T {
        insertCard(options)
        try {
            return block()
        } finally {
            try {
                removeCard()
            } catch (_: DeviceException) {
                try { abort() } catch (_: Exception) {}
            }
        }
    }

    override fun close() {}

    private fun ActionResponse.throwIfError() {
        if (status == "ERROR") throw DeviceException.DeviceErrorException(
            errorCode = error_code ?: "UNKNOWN",
            state = DeviceState.valueOf(this.state),
            message = error_message ?: "",
        )
    }

    companion object {
        fun create(baseUrl: String): HttpCardInserterClient = HttpCardInserterClient(baseUrl)
    }
}

object CardInserterClients {
    @JvmStatic
    fun connect(baseUrl: String): CardInserterClient = HttpCardInserterClient.create(baseUrl)
}
