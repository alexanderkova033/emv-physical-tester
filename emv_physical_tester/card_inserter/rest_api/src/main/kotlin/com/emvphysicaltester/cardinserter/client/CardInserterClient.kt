package com.emvphysicaltester.cardinserter.client

import com.emvphysicaltester.cardinserter.engine.DeviceState

interface CardInserterClient : AutoCloseable {

    @Throws(DeviceException::class)
    fun home(timeoutMillis: Long? = null): HomeResult

    @Throws(DeviceException::class)
    fun insertCard(options: InsertOptions): InsertResult

    @Throws(DeviceException::class)
    fun removeCard(timeoutMillis: Long? = null): RemoveResult

    @Throws(DeviceException::class)
    fun status(): StatusResult

    @Throws(DeviceException::class)
    fun abort(): DeviceState

    @Throws(DeviceException::class)
    fun reset(timeoutMillis: Long? = null): DeviceState

    @Throws(DeviceException::class)
    fun <T> withCardInserted(options: InsertOptions, block: () -> T): T
}
