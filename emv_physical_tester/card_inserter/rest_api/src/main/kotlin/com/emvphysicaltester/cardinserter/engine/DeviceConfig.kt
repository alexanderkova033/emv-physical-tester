package com.emvphysicaltester.cardinserter.engine

data class DeviceConfig(
    val homingTimeMs: Long = 500L,
    val defaultDepthMm: Double = 35.0,
    val defaultSpeedMmPerSecond: Double = 20.0,
    val minDepthMm: Double = 1.0,
    val maxDepthMm: Double = 80.0,
    val minSpeedMmPerSecond: Double = 1.0,
    val maxSpeedMmPerSecond: Double = 200.0,
    val motionTimeoutMs: Long = 30_000L,
)
