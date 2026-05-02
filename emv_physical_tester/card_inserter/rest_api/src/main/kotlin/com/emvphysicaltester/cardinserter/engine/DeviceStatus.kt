package com.emvphysicaltester.cardinserter.engine

data class DeviceStatus(
    val state: DeviceState,
    val lastErrorCode: ErrCode,
    val lastErrorMessage: String,
)
