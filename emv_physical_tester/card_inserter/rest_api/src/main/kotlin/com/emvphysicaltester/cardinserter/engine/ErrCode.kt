package com.emvphysicaltester.cardinserter.engine

enum class ErrCode {
    NONE,
    ILLEGAL_STATE,
    HOME_FAILED,
    MOTION_TIMEOUT,
    CARD_JAM,
    SENSOR_FAULT,
    PROTOCOL_ERROR,
    INTERNAL_ERROR,
    ESTOP_ASSERTED,
    UNSAFE_CONFIGURATION,
}
