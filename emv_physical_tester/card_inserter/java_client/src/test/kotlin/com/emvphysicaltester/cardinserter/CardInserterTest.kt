package com.emvphysicaltester.cardinserter

import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

/**
 * Set CARD_INSERTER_URL to the device base URL before running, e.g.:
 *   CARD_INSERTER_URL=http://192.168.1.42 ./gradlew test
 */
class CardInserterTest {

    private val baseUrl = System.getenv("CARD_INSERTER_URL") ?: "http://localhost:8180"

    private lateinit var client: CardInserterClient

    @BeforeEach
    fun setUp() {
        client = CardInserterClients.connect(baseUrl)
        client.home()
    }

    @AfterEach
    fun tearDown() {
        runCatching { client.removeCard() }
        client.close()
    }

    @Test
    fun `insert card reaches INSERTED state`() {
        val result = client.insertCard(InsertOptions(depthMm = 40))
        assertEquals(DeviceState.INSERTED, result.state)
    }

    @Test
    fun `remove card returns to IDLE`() {
        client.insertCard(InsertOptions(depthMm = 40))
        val result = client.removeCard()
        assertEquals(DeviceState.IDLE, result.state)
    }
}
