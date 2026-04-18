package com.emvphysicaltester.cardinserter.engine

sealed class SseEvent {
    abstract val type: String

    data class StateChanged(
        val old_state: String,
        val new_state: String,
    ) : SseEvent() {
        override val type: String get() = "STATE_CHANGED"
    }

    data class Fault(
        val error_code: String,
        val error_message: String,
    ) : SseEvent() {
        override val type: String get() = "FAULT"
    }

    data class Reservation(
        val owner: String,
        val action: String,
    ) : SseEvent() {
        override val type: String get() = "RESERVATION"
    }
}
