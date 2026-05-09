package com.emvphysicaltester.cardinserter

import com.fasterxml.jackson.databind.JsonNode
import com.fasterxml.jackson.module.kotlin.jacksonObjectMapper
import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest
import java.net.http.HttpResponse
import java.time.Duration

internal class HttpCardInserterClient(
    private val baseUrl: String,
    private val timeoutMillis: Long
) : CardInserterClient {

    private val http = HttpClient.newBuilder()
        .connectTimeout(Duration.ofMillis(timeoutMillis))
        .build()

    private val mapper = jacksonObjectMapper()

    override fun home(): DeviceState {
        val node = post("/api/home", mapOf<String, Any>())
        return DeviceState.fromString(node["state"].asText())
    }

    override fun insertCard(options: InsertOptions): InsertResult {
        val body = buildMap<String, Any> {
            put("depth_mm", options.depthMm)
            if (options.speedMmPerSecond != null) put("speed_mm_s", options.speedMmPerSecond)
        }
        val node = post("/api/insert", body)
        return InsertResult(
            state = DeviceState.fromString(node["state"].asText()),
            motionTimeMillis = node["motion_time_ms"]?.asLong()
        )
    }

    override fun removeCard(): RemoveResult {
        val node = post("/api/remove", mapOf<String, Any>())
        return RemoveResult(
            state = DeviceState.fromString(node["state"].asText()),
            motionTimeMillis = node["motion_time_ms"]?.asLong()
        )
    }

    override fun status(): StatusResult {
        val node = get("/api/status")
        return StatusResult(
            state = DeviceState.fromString(node["state"].asText()),
            lastErrorCode = node["last_error_code"]?.asText() ?: "NONE",
            protocolVersion = node["protocol_version"]?.asInt() ?: 1
        )
    }

    override fun abort(): DeviceState {
        val node = post("/api/abort", mapOf<String, Any>())
        return DeviceState.fromString(node["state"].asText())
    }

    private fun post(path: String, body: Map<String, Any>): JsonNode {
        val request = HttpRequest.newBuilder()
            .uri(URI.create("$baseUrl$path"))
            .timeout(Duration.ofMillis(timeoutMillis))
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(mapper.writeValueAsString(body)))
            .build()
        return send(request)
    }

    private fun get(path: String): JsonNode {
        val request = HttpRequest.newBuilder()
            .uri(URI.create("$baseUrl$path"))
            .timeout(Duration.ofMillis(timeoutMillis))
            .GET()
            .build()
        return send(request)
    }

    private fun send(request: HttpRequest): JsonNode {
        val response = try {
            http.send(request, HttpResponse.BodyHandlers.ofString())
        } catch (e: Exception) {
            throw ConnectionException("Failed to connect to $baseUrl: ${e.message}", e)
        }

        val node = mapper.readTree(response.body())
        if (node["status"]?.asText() != "OK") {
            val errorCode = node["error_code"]?.asText() ?: "UNKNOWN_ERROR"
            val state = DeviceState.fromString(node["state"]?.asText() ?: "UNKNOWN")
            val message = node["error_message"]?.asText()?.takeIf { it.isNotBlank() } ?: errorCode
            throw DeviceErrorException(errorCode, state, message)
        }
        return node
    }
}
