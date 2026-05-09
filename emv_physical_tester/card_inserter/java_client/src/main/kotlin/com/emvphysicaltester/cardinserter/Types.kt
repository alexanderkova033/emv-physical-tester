package com.emvphysicaltester.cardinserter

enum class DeviceState {
    BOOTING, HOMING, IDLE, INSERTING, INSERTED, REMOVING, ERROR, UNKNOWN;

    companion object {
        @JvmStatic
        fun fromString(value: String): DeviceState =
            entries.firstOrNull { it.name.equals(value, ignoreCase = true) } ?: UNKNOWN
    }
}

data class InsertOptions @JvmOverloads constructor(
    val depthMm: Int,
    val speedMmPerSecond: Int? = null,
    val timeoutMillis: Long = 30_000L
)

data class InsertResult(val state: DeviceState, val motionTimeMillis: Long?)

data class RemoveResult(val state: DeviceState, val motionTimeMillis: Long?)

data class StatusResult(
    val state: DeviceState,
    val lastErrorCode: String,
    val protocolVersion: Int
)

open class DeviceException(message: String, cause: Throwable? = null) : RuntimeException(message, cause)

class DeviceErrorException(
    val errorCode: String,
    val state: DeviceState,
    message: String
) : DeviceException(message)

class ConnectionException(message: String, cause: Throwable? = null) : DeviceException(message, cause)
