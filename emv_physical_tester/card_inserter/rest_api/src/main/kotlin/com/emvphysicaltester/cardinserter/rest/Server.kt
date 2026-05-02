package com.emvphysicaltester.cardinserter.rest

import com.emvphysicaltester.cardinserter.engine.DeviceEngine
import com.sun.net.httpserver.HttpServer
import java.net.InetSocketAddress
import java.util.concurrent.Executors

fun createServer(engine: DeviceEngine, port: Int = 8080): HttpServer {
    val server = HttpServer.create(InetSocketAddress(port), 0)
    server.executor = Executors.newCachedThreadPool()
    server.registerRoutes(engine)
    return server
}
