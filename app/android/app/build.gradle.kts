plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.selfso.robohero"
    compileSdk = 35

    val readmeFile = rootProject.projectDir.resolve("../../README.md")
    val versionRegex = Regex("""(?:<!--\s*robohero-version:\s*|\*\*Version:\s*)([0-9]+)\.([0-9]+)\.([0-9]+)""", RegexOption.IGNORE_CASE)
    val versionMatch = versionRegex.find(if (readmeFile.exists()) readmeFile.readText() else "")
    val (projMajor, projMinor, projPatch) = if (versionMatch != null) {
        Triple(versionMatch.groupValues[1], versionMatch.groupValues[2], versionMatch.groupValues[3])
    } else {
        Triple("2", "1", "15")
    }
    val projVersionName = "$projMajor.$projMinor.$projPatch"
    val projVersionCode = projMajor.toInt() * 10000 + projMinor.toInt() * 100 + projPatch.toInt()

    defaultConfig {
        applicationId = "com.selfso.robohero"
        minSdk = 26
        targetSdk = 35
        versionCode = projVersionCode
        versionName = projVersionName

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
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
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }
    kotlinOptions {
        jvmTarget = "21"
    }
    buildFeatures {
        compose = true
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    implementation(libs.androidx.material.icons.extended)
    implementation(libs.okhttp)
    implementation(libs.paho.mqtt)

    debugImplementation(libs.androidx.ui.tooling)
}
