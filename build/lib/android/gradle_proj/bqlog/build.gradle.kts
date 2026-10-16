plugins {
    id("com.android.library")
}

android {
    namespace = "com.tencent.bqlog"
    compileSdk = 36

    buildFeatures {
        prefabPublishing = true
    }

    prefab {
        create("BqLog") {
            headers = "../../../../../artifacts/dynamic_lib/include"
            libraryName = "libBqLog"
        }
    }

    defaultConfig {
        minSdk = 21

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            debugSymbolLevel = "FULL"
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DBUILD_LIB_TYPE=dynamic_lib", "-DTARGET_PLATFORM:STRING=android", "-DANDROID_STL=none", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            isMinifyEnabled = false
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("../../../../../src/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }

    sourceSets {
        named("main") {
            java.srcDirs("../../../../../wrapper/java/src")
            manifest.srcFile("src/main/AndroidManifest.xml")
        }
    }
}

fun createCopyAarTask(taskName: String, buildType: String, relativePath: String) {
    tasks.register<Copy>(taskName) {
        val aarName = "bqlog-$buildType.aar"
        val outputDir = layout.buildDirectory.dir("outputs/aar").get().asFile
        val destDir = rootProject.file(relativePath)

        from(outputDir) {
            include(aarName)
        }
        into(destDir)
        doFirst {
            destDir.mkdirs()
        }
    }
}

// Javadoc task for the Android AAR (sources live in wrapper/java/src)
val androidJavadoc by tasks.registering(Javadoc::class) {
    source = fileTree("../../../../../wrapper/java/src") { include("**/*.java") }
    classpath += files(android.bootClasspath)
    setDestinationDir(layout.buildDirectory.dir("outputs/javadoc").get().asFile)
    options {
        encoding = "UTF-8"
        (this as StandardJavadocDocletOptions).charSet("UTF-8")
    }
    isFailOnError = false
}

val androidJavadocJar by tasks.registering(Jar::class) {
    dependsOn(androidJavadoc)
    archiveClassifier.set("javadoc")
    from(androidJavadoc.get().destinationDir)
    archiveBaseName.set("bqlog")
    archiveVersion.set(project.version.toString())
}

fun createCopyJavadocJarTask(taskName: String, relativePath: String) {
    tasks.register<Copy>(taskName) {
        dependsOn(androidJavadocJar)
        val destDir = rootProject.file(relativePath)
        from(androidJavadocJar.get().archiveFile)
        into(destDir)
        doFirst {
            destDir.mkdirs()
        }
    }
}

createCopyAarTask("copyAarRelease", "release", "../../../../install/dynamic_lib")
createCopyAarTask("copyAarDebug", "debug", "../../../../install/dynamic_lib")
createCopyJavadocJarTask("copyJavadocJar", "../../../../install/dynamic_lib")

tasks.named("assemble") {
    finalizedBy("copyAarRelease")
    finalizedBy("copyAarDebug")
    finalizedBy("copyJavadocJar")
}
