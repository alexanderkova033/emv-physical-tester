plugins {
    base
}

val kotlinLib = "/usr/local/sdkman/candidates/gradle/current/lib"
val kotlinCompilerCp = fileTree(kotlinLib) {
    include("kotlin*.jar", "kotlinx-coroutines-core-jvm*.jar")
}.asPath

val runtimeCp = listOf(
    "$kotlinLib/kotlin-stdlib-2.3.0.jar",
    "$kotlinLib/kotlinx-coroutines-core-jvm-1.10.2.jar",
    "$kotlinLib/gson-2.13.1.jar",
    "$kotlinLib/annotations-24.0.1.jar",
).joinToString(File.pathSeparator)

val mainOutput = file("build/classes/main")
val testOutput = file("build/classes/test")

val mainSources = fileTree("src/main/kotlin") { include("**/*.kt") }
val testSources = fileTree("src/test/kotlin") { include("**/*.kt") }

tasks.register<JavaExec>("compileMain") {
    group = "build"
    inputs.files(mainSources)
    outputs.dir(mainOutput)
    classpath = fileTree(kotlinLib) { include("kotlin*.jar", "kotlinx*.jar", "annotations*.jar") }
    mainClass.set("org.jetbrains.kotlin.cli.jvm.K2JVMCompiler")
    doFirst {
        mainOutput.mkdirs()
        args(
            "-no-stdlib",
            "-classpath", runtimeCp,
            "-d", mainOutput.absolutePath,
            *mainSources.map { it.absolutePath }.toTypedArray(),
        )
    }
}

tasks.register<JavaExec>("compileTest") {
    group = "build"
    dependsOn("compileMain")
    inputs.files(testSources)
    outputs.dir(testOutput)
    classpath = fileTree(kotlinLib) { include("kotlin*.jar", "kotlinx*.jar", "annotations*.jar") }
    mainClass.set("org.jetbrains.kotlin.cli.jvm.K2JVMCompiler")
    doFirst {
        testOutput.mkdirs()
        args(
            "-no-stdlib",
            "-classpath", "$runtimeCp${File.pathSeparator}${mainOutput.absolutePath}",
            "-d", testOutput.absolutePath,
            *testSources.map { it.absolutePath }.toTypedArray(),
        )
    }
}

tasks.register<JavaExec>("runTests") {
    group = "verification"
    description = "Compile and run all tests"
    dependsOn("compileTest")
    classpath = files(runtimeCp.split(File.pathSeparator)) + files(mainOutput, testOutput)
    mainClass.set("com.emvphysicaltester.cardinserter.TestRunnerKt")
}

tasks.named("check") {
    dependsOn("runTests")
}
