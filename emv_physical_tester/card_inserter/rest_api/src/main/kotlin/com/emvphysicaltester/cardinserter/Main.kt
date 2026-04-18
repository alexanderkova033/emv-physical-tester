package com.emvphysicaltester.cardinserter

import com.emvphysicaltester.cardinserter.engine.DeviceEngine
import com.emvphysicaltester.cardinserter.rest.createServer

fun main() {
    val engine = DeviceEngine()
    engine.home()
    val server = createServer(engine, port = 8080)
    server.start()
    println("Card inserter REST API running on http://localhost:8080")
    Thread.currentThread().join()
}
