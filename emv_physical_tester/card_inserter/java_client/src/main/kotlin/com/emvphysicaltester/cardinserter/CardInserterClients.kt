package com.emvphysicaltester.cardinserter

object CardInserterClients {

    @JvmStatic
    @JvmOverloads
    fun connect(baseUrl: String, timeoutMillis: Long = 30_000L): CardInserterClient =
        HttpCardInserterClient(baseUrl.trimEnd('/'), timeoutMillis)
}
