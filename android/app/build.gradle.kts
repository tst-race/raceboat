plugins {
    id("com.android.application")
}

android {
    namespace = "com.example.raceboat"
    compileSdk = 33
    //compileSdk = 34

    defaultConfig {
        applicationId = "com.example.raceboat"
        minSdk = 33
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                arguments += "-DBUILD_VERSION=dev"
                arguments += "-DPYTHON_INCLUDE_DIR=$projectDir/src/main/python/python-3.7.16-3-android-arm64-v8a/include/python3.7m"
                arguments += "-DPYTHON_LIBRARY=$projectDir/src/main/jniLibs/arm64-v8a/libpython3.7m.so"
                arguments += "-DPYTHON_EXECUTABLE=$projectDir/src/main/jniLibs/arm64-v8a/python3.7m"
                cppFlags += "-std=c++17"
                cppFlags += "-I$projectDir/src/main/cpp/include"
            }
        }
        //ndk{
        //    abiFilters = new MutableSet<String>("")
        //}
        ndk {
            //abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            abiFilters += listOf( "arm64-v8a")
            //"armeabi-v7a", "x86", "x86_64")
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
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.22.1"
            //version = "3.13.4"
        }
    }
    buildFeatures {
        viewBinding = true
    }
}

dependencies {

    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.9.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}
