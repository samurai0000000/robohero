package com.selfso.robohero.data

import android.content.Context
import android.content.SharedPreferences

class PreferencesManager(context: Context) {
    private val prefs: SharedPreferences =
        context.getSharedPreferences("robohero_prefs", Context.MODE_PRIVATE)

    var robotIp: String
        get() = prefs.getString(KEY_ROBOT_IP, "") ?: ""
        set(value) = prefs.edit().putString(KEY_ROBOT_IP, value).apply()

    var robotPort: Int
        get() = prefs.getInt(KEY_ROBOT_PORT, 80)
        set(value) = prefs.edit().putInt(KEY_ROBOT_PORT, value).apply()

    var mqttEnabled: Boolean
        get() = prefs.getBoolean(KEY_MQTT_ENABLED, false)
        set(value) = prefs.edit().putBoolean(KEY_MQTT_ENABLED, value).apply()

    var mqttHost: String
        get() = prefs.getString(KEY_MQTT_HOST, "") ?: ""
        set(value) = prefs.edit().putString(KEY_MQTT_HOST, value).apply()

    var mqttPort: Int
        get() = prefs.getInt(KEY_MQTT_PORT, 1883)
        set(value) = prefs.edit().putInt(KEY_MQTT_PORT, value).apply()

    var mqttUser: String
        get() = prefs.getString(KEY_MQTT_USER, "") ?: ""
        set(value) = prefs.edit().putString(KEY_MQTT_USER, value).apply()

    var mqttPass: String
        get() = prefs.getString(KEY_MQTT_PASS, "") ?: ""
        set(value) = prefs.edit().putString(KEY_MQTT_PASS, value).apply()

    var mqttClientId: String
        get() = prefs.getString(KEY_MQTT_CLIENT_ID, "") ?: ""
        set(value) = prefs.edit().putString(KEY_MQTT_CLIENT_ID, value).apply()

    companion object {
        private const val KEY_ROBOT_IP = "robot_ip"
        private const val KEY_ROBOT_PORT = "robot_port"
        private const val KEY_MQTT_ENABLED = "mqtt_enabled"
        private const val KEY_MQTT_HOST = "mqtt_host"
        private const val KEY_MQTT_PORT = "mqtt_port"
        private const val KEY_MQTT_USER = "mqtt_user"
        private const val KEY_MQTT_PASS = "mqtt_pass"
        private const val KEY_MQTT_CLIENT_ID = "mqtt_client_id"
    }
}
