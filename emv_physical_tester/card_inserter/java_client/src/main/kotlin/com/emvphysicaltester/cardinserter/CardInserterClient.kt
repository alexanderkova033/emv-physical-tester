package com.emvphysicaltester.cardinserter

interface CardInserterClient : AutoCloseable {

    @Throws(DeviceException::class)
    fun home(): DeviceState

    @Throws(DeviceException::class)
    fun insertCard(options: InsertOptions): InsertResult

    @Throws(DeviceException::class)
    fun removeCard(): RemoveResult

    @Throws(DeviceException::class)
    fun status(): StatusResult

    @Throws(DeviceException::class)
    fun abort(): DeviceState

    override fun close() {}
}
