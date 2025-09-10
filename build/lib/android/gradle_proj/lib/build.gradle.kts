plugins {
    id("com.android.library")
}

android {
    namespace = "bq"
    compileSdk = 36

    defaultConfig {
        minSdk = 21
        targetSdk = 36

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            // "NONE", "SYMBOL_TABLE", "FULL"
            debugSymbolLevel = "FULL"
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DBUILD_TYPE=dynamic_lib", "-DTARGET_PLATFORM:STRING=android", "-DANDROID_STL=none")
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
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    externalNativeBuild {
        cmake {
            path = file("../../../../../src/CMakeLists.txt")
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