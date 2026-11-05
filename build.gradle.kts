plugins {
    application
    java
    id("org.openjfx.javafxplugin") version "0.1.0"
}

group = "net.stratalauncher"
version = "0.1.0"

repositories {
    mavenCentral()
}

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(21))
    }
}

val testJavaVersion = providers.gradleProperty("targetJava")
    .map(String::toInt)
    .getOrElse(21)
require(testJavaVersion == 21 || testJavaVersion == 26) {
    "targetJava must be 21 or 26."
}

javafx {
    version = "21.0.8"
    modules = listOf("javafx.controls", "javafx.fxml")
}

application {
    mainClass.set("net.stratalauncher.LauncherBootstrap")
}

tasks.named<JavaExec>("run") {
    doFirst {
        val debugOverlay = providers.gradleProperty("debugFrameTime").map(String::toBoolean).getOrElse(false)
        systemProperty("stratalauncher.debugFrameTime", debugOverlay.toString())
    }
}

tasks.register<Exec>("jpackage") {
    group = "distribution"
    description = "Creates a platform-specific StrataLauncher application image."
    dependsOn(tasks.named("installDist"))
    doFirst {
        delete(layout.buildDirectory.dir("jpackage/StrataLauncher"))
        val input = layout.buildDirectory.dir("install/${project.name}/lib").get().asFile
        val mainJar = tasks.jar.get().archiveFile.get().asFile
        commandLine(
            "jpackage",
            "--type", "app-image",
            "--dest", layout.buildDirectory.dir("jpackage").get().asFile.absolutePath,
            "--name", "StrataLauncher",
            "--input", input.absolutePath,
            "--main-jar", mainJar.name,
            "--main-class", "net.stratalauncher.LauncherBootstrap"
        )
    }
}

tasks.withType<JavaCompile>().configureEach {
    options.encoding = "UTF-8"
    options.release.set(21)
}

dependencies {
    implementation("io.github.mkpaz:atlantafx-base:2.0.1")
    implementation("io.github.mkpaz:atlantafx-styles:2.0.1")
    implementation("com.google.code.gson:gson:2.13.2")
    implementation("com.github.javakeyring:java-keyring:1.0.4")
    implementation("org.apache.commons:commons-compress:1.28.0")

    testImplementation(platform("org.junit:junit-bom:5.10.2"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.test {
    useJUnitPlatform()
    javaLauncher = javaToolchains.launcherFor {
        languageVersion.set(JavaLanguageVersion.of(testJavaVersion))
    }
}
