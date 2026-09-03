# Add project specific ProGuard rules here.

# Paho MQTT Client
-keep class org.eclipse.paho.client.mqttv3.** { *; }
-dontwarn org.eclipse.paho.client.mqttv3.**

# OkHttp
-dontwarn okhttp3.**
-dontwarn okio.**
-keepnames class okhttp3.internal.publicsuffix.PublicSuffixDatabase

# Coroutines
-keepnames class kotlinx.coroutines.internal.MainDispatcherFactory { *; }
-keepnames class kotlinx.coroutines.CoroutineExceptionHandler { *; }
-dontwarn kotlinx.coroutines.**

# Compose
-keepclassmembers class * {
    @androidx.compose.runtime.Composable *;
}
