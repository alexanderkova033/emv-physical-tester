package com.emvphysicaltester.cardinserter

fun assertEquals(expected: Any?, actual: Any?, msg: String = "") {
    if (expected != actual)
        throw AssertionError("Expected <$expected> but was <$actual>${if (msg.isNotEmpty()) ": $msg" else ""}")
}

fun assertNotNull(value: Any?, msg: String = "") {
    if (value == null) throw AssertionError("Expected non-null${if (msg.isNotEmpty()) ": $msg" else ""}")
}

fun assertNull(value: Any?, msg: String = "") {
    if (value != null) throw AssertionError("Expected null but was <$value>${if (msg.isNotEmpty()) ": $msg" else ""}")
}

fun assertTrue(cond: Boolean, msg: String = "Expected true") {
    if (!cond) throw AssertionError(msg)
}

fun assertFalse(cond: Boolean, msg: String = "Expected false") {
    if (cond) throw AssertionError(msg)
}

fun <T : Exception> assertThrows(type: Class<T>, block: () -> Unit): T {
    try {
        block()
        throw AssertionError("Expected ${type.simpleName} to be thrown")
    } catch (e: AssertionError) {
        throw e
    } catch (e: Exception) {
        if (!type.isInstance(e))
            throw AssertionError("Expected ${type.simpleName} but got ${e.javaClass.simpleName}: ${e.message}")
        @Suppress("UNCHECKED_CAST")
        return e as T
    }
}
