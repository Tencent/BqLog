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

createCopyAarTask("copyAarRelease", "release", "../../../../install/dynamic_lib")
createCopyAarTask("copyAarDebug", "debug", "../../../../install/dynamic_lib")

tasks.named("assemble") {
    finalizedBy("copyAarRelease")
    finalizedBy("copyAarDebug")
}

