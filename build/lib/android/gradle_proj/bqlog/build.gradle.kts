plugins {
    id("com.android.library")
}

android {
    namespace = "com.tencent.bqlog"
    compileSdk = 36

    defaultConfig {
        minSdk = 21

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            // "NONE", "SYMBOL_TABLE", "FULL"
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
    
    packagingOptions {
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

fun dependencies() {
    // Add your dependencies here if needed
}

// 创建一个函数来生成复制AAR的任务
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

// 创建任务
createCopyAarTask("copyAarRelease", "release", "../../../../install/dynamic_lib")
createCopyAarTask("copyAarDebug", "debug", "../../../../install/dynamic_lib")

// 配置assemble任务
tasks.named("assemble") {
    finalizedBy("copyAarRelease")
    finalizedBy("copyAarDebug")
}