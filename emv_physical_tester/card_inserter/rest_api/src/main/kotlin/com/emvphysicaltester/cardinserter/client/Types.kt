package com.emvphysicaltester.cardinserter.client

import com.emvphysicaltester.cardinserter.engine.DeviceState

@JvmInline value class Millimeters(val value: Double)
@JvmInline value class MillimetersPerSecond(val value: Double)

data class InsertOptions(
    val depth: Millimeters,
    val speed: MillimetersPerSecond? = null,
    val timeoutMillis: Long? = null,
)

data class InsertResult(val state: DeviceState, val motionTimeMillis: Long?)
data class RemoveResult(val state: DeviceState, val motionTimeMillis: Long?)
data class HomeResult(val state: DeviceState)

data class StatusResult(
    val state: DeviceState,
    val lastErrorCode: String,
    val lastErrorMessage: String,
    val protocolVersion: Int,
    val minCompatibleProtocolVersion: Int,
    val features: List<String>,
)

sealed class DeviceException(message: String, cause: Throwable? = null) : Exception(message, cause) {
    class DeviceErrorException(val errorCode: String, val state: DeviceState, message: String)
        : DeviceException("[$errorCode] in state $state: $message")
    class ProtocolException(message: String) : DeviceException(message)
    class ConnectionException(message: String, cause: Throwable? = null) : DeviceException(message, cause)
}
