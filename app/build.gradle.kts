plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "net.sorayuki.featuretest"
    compileSdk {
        version = release(36)
    }

    defaultConfig {
        applicationId = "net.sorayuki.featuretest"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
        externalNativeBuild {
            cmake {
                cppFlags += ""
            }
        }
        ndk {
            // abiFilters += "arm64-v8a"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    kotlinOptions {
        jvmTarget = "11"
    }
    buildFeatures {
        viewBinding = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    // 这里KHR提供的源码编译出来的so不知道为什么不能用、不能读取/lib/vendor下对应的so
    // 所以要排除掉它编出来的，转而声明 uses-native-library 然后使用系统的
    packaging {
        jniLibs {
            excludes += "/lib/**/libOpenCL.so"
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.activity)
    implementation(libs.androidx.constraintlayout)
}