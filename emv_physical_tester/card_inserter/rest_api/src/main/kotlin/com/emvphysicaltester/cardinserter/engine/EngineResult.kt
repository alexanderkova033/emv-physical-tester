package com.emvphysicaltester.cardinserter.engine

data class EngineResult(
    val state: DeviceState,
    val motionTimeMs: Long = 0L,
    val errorCode: ErrCode = ErrCode.NONE,
    val errorMessage: String = "",
) {
    val isSuccess: Boolean get() = errorCode == ErrCode.NONE
}
