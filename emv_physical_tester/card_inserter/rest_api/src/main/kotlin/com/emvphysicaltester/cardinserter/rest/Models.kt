package com.emvphysicaltester.cardinserter.rest

import com.google.gson.JsonElement

data class HomeRequest(val id: JsonElement? = null)

data class InsertRequest(
    val id: JsonElement? = null,
    val depth_mm: Double? = null,
    val speed_mm_s: Double? = null,
)

data class RemoveRequest(val id: JsonElement? = null)

data class AbortRequest(val id: JsonElement? = null)

data class ResetRequest(val id: JsonElement? = null)

data class ActionResponse(
    val id: JsonElement? = null,
    val status: String,
    val state: String,
    val motion_time_ms: Long? = null,
    val error_code: String? = null,
    val error_message: String? = null,
)

data class StatusResponse(
    val id: JsonElement? = null,
    val status: String,
    val state: String,
    val last_error_code: String,
    val last_error_message: String,
    val protocol_version: Int = 1,
    val min_compatible_protocol_version: Int = 1,
    val features: List<String> = listOf("EVENTS", "RESET"),
)
