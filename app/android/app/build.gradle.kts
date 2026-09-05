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

    signingConfigs {
        create("release") {
            val storeFilePath = project.findProperty("RELEASE_STORE_FILE") as? String
                ?: System.getenv("RELEASE_STORE_FILE")
            val storePass = project.findProperty("RELEASE_STORE_PASSWORD") as? String
                ?: System.getenv("RELEASE_STORE_PASSWORD")
            val keyUser = project.findProperty("RELEASE_KEY_ALIAS") as? String
                ?: System.getenv("RELEASE_KEY_ALIAS")
            val keyPass = project.findProperty("RELEASE_KEY_PASSWORD") as? String
                ?: System.getenv("RELEASE_KEY_PASSWORD")

            if (storeFilePath != null && file(storeFilePath).exists() && storePass != null) {
                storeFile = file(storeFilePath)
                storePassword = storePass
                keyAlias = keyUser ?: "selfso"
                keyPassword = keyPass ?: storePass
            } else {
                initWith(getByName("debug"))
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            signingConfig = signingConfigs.getByName("release")
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    applicationVariants.all {
        outputs.all {
            val outputImpl = this as? com.android.build.gradle.internal.api.BaseVariantOutputImpl
            if (outputImpl != null) {
                val suffix = if (buildType.name == "release") "" else "-${buildType.name}"
                outputImpl.outputFileName = "RoboHero-v${projVersionName}${suffix}.apk"
            }
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
    sourceSets {
        getByName("main") {
            assets.srcDirs(
                "src/main/assets",
                "../../model"
            )
        }
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
