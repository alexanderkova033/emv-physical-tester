package com.emvphysicaltester.cardinserter.engine

import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.Semaphore

class DeviceEngine(
    val config: DeviceConfig = DeviceConfig(),
    private val delayFn: (Long) -> Unit = { ms -> if (ms > 0) Thread.sleep(ms) },
) {
    @Volatile private var state: DeviceState = DeviceState.BOOTING
    @Volatile private var lastErrorCode: ErrCode = ErrCode.NONE
    @Volatile private var lastErrorMessage: String = "NONE"
    @Volatile private var abortRequested: Boolean = false

    private val operationLock = Semaphore(1)
    private val eventListeners = CopyOnWriteArrayList<(SseEvent) -> Unit>()

    val currentState: DeviceState get() = state

    fun getStatus() = DeviceStatus(state, lastErrorCode, lastErrorMessage)

    fun addEventListener(listener: (SseEvent) -> Unit) { eventListeners.add(listener) }
    fun removeEventListener(listener: (SseEvent) -> Unit) { eventListeners.remove(listener) }

    private fun transition(new: DeviceState) {
        val old = state
        state = new
        if (old != new) publishEvent(SseEvent.StateChanged(old.name, new.name))
    }

    private fun enterError(code: ErrCode, message: String) {
        lastErrorCode = code
        lastErrorMessage = message
        transition(DeviceState.ERROR)
        publishEvent(SseEvent.Fault(code.name, message))
    }

    private fun publishEvent(event: SseEvent) = eventListeners.forEach { it(event) }

    fun home(): EngineResult {
        if (state == DeviceState.ESTOP)
            return EngineResult(state, errorCode = ErrCode.ESTOP_ASSERTED, errorMessage = "E-stop is asserted")
        if (!operationLock.tryAcquire())
            return EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Another operation is in progress")
        return try {
            abortRequested = false
            transition(DeviceState.HOMING)
            val start = System.currentTimeMillis()
            if (!sleepInterruptible(config.homingTimeMs)) {
                enterError(ErrCode.HOME_FAILED, "Homing aborted")
                return EngineResult(DeviceState.ERROR, System.currentTimeMillis() - start, ErrCode.HOME_FAILED, "Homing aborted")
            }
            lastErrorCode = ErrCode.NONE
            lastErrorMessage = "NONE"
            transition(DeviceState.IDLE)
            EngineResult(DeviceState.IDLE, System.currentTimeMillis() - start)
        } finally {
            operationLock.release()
        }
    }

    fun insert(depthMm: Double, speedMmPerSecond: Double? = null): EngineResult {
        if (state == DeviceState.ESTOP)
            return EngineResult(state, errorCode = ErrCode.ESTOP_ASSERTED, errorMessage = "E-stop is asserted")
        if (state != DeviceState.IDLE)
            return EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Device must be IDLE to insert, was ${state.name}")
        if (depthMm < config.minDepthMm || depthMm > config.maxDepthMm)
            return EngineResult(state, errorCode = ErrCode.UNSAFE_CONFIGURATION,
                errorMessage = "depth_mm $depthMm out of safe range [${config.minDepthMm}, ${config.maxDepthMm}]")
        val speed = speedMmPerSecond ?: config.defaultSpeedMmPerSecond
        if (speed < config.minSpeedMmPerSecond || speed > config.maxSpeedMmPerSecond)
            return EngineResult(state, errorCode = ErrCode.UNSAFE_CONFIGURATION,
                errorMessage = "speed_mm_s $speed out of safe range [${config.minSpeedMmPerSecond}, ${config.maxSpeedMmPerSecond}]")

        if (!operationLock.tryAcquire())
            return EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Another operation is in progress")
        return try {
            if (state != DeviceState.IDLE)
                return EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Device must be IDLE to insert")
            abortRequested = false
            transition(DeviceState.INSERTING)
            val start = System.currentTimeMillis()
            val motionMs = (depthMm / speed * 1000.0).toLong()
            if (!sleepInterruptible(motionMs)) {
                transition(DeviceState.IDLE)
                return EngineResult(DeviceState.IDLE, System.currentTimeMillis() - start)
            }
            if (System.currentTimeMillis() - start > config.motionTimeoutMs) {
                enterError(ErrCode.MOTION_TIMEOUT, "Insert motion exceeded timeout")
                return EngineResult(DeviceState.ERROR, System.currentTimeMillis() - start, ErrCode.MOTION_TIMEOUT, "Motion timeout")
            }
            transition(DeviceState.INSERTED)
            EngineResult(DeviceState.INSERTED, System.currentTimeMillis() - start)
        } finally {
            operationLock.release()
        }
    }

    fun remove(): EngineResult {
        if (state == DeviceState.ESTOP)
            return EngineResult(state, errorCode = ErrCode.ESTOP_ASSERTED, errorMessage = "E-stop is asserted")
        if (state != DeviceState.INSERTED)
            return EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Device must be INSERTED to remove, was ${state.name}")

        if (!operationLock.tryAcquire())
            return EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Another operation is in progress")
        return try {
            if (state != DeviceState.INSERTED)
                return EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Device must be INSERTED to remove")
            abortRequested = false
            transition(DeviceState.REMOVING)
            val start = System.currentTimeMillis()
            val motionMs = (config.defaultDepthMm / config.defaultSpeedMmPerSecond * 1000.0).toLong()
            if (!sleepInterruptible(motionMs)) {
                transition(DeviceState.IDLE)
                return EngineResult(DeviceState.IDLE, System.currentTimeMillis() - start)
            }
            transition(DeviceState.IDLE)
            EngineResult(DeviceState.IDLE, System.currentTimeMillis() - start)
        } finally {
            operationLock.release()
        }
    }

    fun abort(): EngineResult {
        abortRequested = true
        val deadline = System.currentTimeMillis() + 2000L
        while (operationLock.availablePermits() == 0 && System.currentTimeMillis() < deadline) {
            Thread.sleep(5)
        }
        return EngineResult(state)
    }

    fun reset(): EngineResult = when (state) {
        DeviceState.ESTOP -> EngineResult(state, errorCode = ErrCode.ESTOP_ASSERTED, errorMessage = "E-stop still asserted")
        DeviceState.ERROR -> home()
        DeviceState.IDLE -> EngineResult(DeviceState.IDLE)
        else -> EngineResult(state, errorCode = ErrCode.ILLEGAL_STATE, errorMessage = "Cannot reset from ${state.name}")
    }

    fun assertEStop() {
        state = DeviceState.ESTOP
        publishEvent(SseEvent.Fault(ErrCode.ESTOP_ASSERTED.name, "Emergency stop asserted"))
    }

    fun releaseEStop() {
        if (state == DeviceState.ESTOP) {
            lastErrorCode = ErrCode.ESTOP_ASSERTED
            lastErrorMessage = "E-stop was asserted"
            transition(DeviceState.ERROR)
        }
    }

    private fun sleepInterruptible(totalMs: Long): Boolean {
        if (totalMs <= 0L) return !abortRequested
        val pollMs = minOf(10L, totalMs)
        var remaining = totalMs
        while (remaining > 0L) {
            if (abortRequested) return false
            val step = minOf(pollMs, remaining)
            delayFn(step)
            remaining -= step
        }
        return !abortRequested
    }
}
