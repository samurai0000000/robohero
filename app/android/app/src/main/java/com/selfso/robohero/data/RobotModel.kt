package com.selfso.robohero.data

enum class ConnectionStatus {
    DISCONNECTED,
    SEARCHING,
    CONNECTED_HTTP,
    CONNECTED_MQTT,
    ERROR
}

data class TrimItem(
    val id: Int,
    val name: String,
    val category: String,
    val min: Int = -125,
    val max: Int = 125,
    var value: Int = 0
)

data class MotionCommand(
    val key: String,
    val value: Int,
    val label: String
)

object TrimDefinitions {
    fun defaultTrims(): List<TrimItem> = listOf(
        // Right Leg
        TrimItem(0, "Servo 0 - R Ankle Roll", "Right Leg"),
        TrimItem(1, "Servo 1 - R Ankle Pitch", "Right Leg"),
        TrimItem(2, "Servo 2 - R Knee", "Right Leg"),
        TrimItem(3, "Servo 3 - R Hip Pitch", "Right Leg"),
        TrimItem(4, "Servo 4 - R Hip Roll", "Right Leg"),

        // Left Leg
        TrimItem(5, "Servo 5 - L Hip Roll", "Left Leg"),
        TrimItem(6, "Servo 6 - L Hip Pitch", "Left Leg"),
        TrimItem(7, "Servo 7 - L Knee", "Left Leg"),
        TrimItem(8, "Servo 8 - L Ankle Pitch", "Left Leg"),
        TrimItem(9, "Servo 9 - L Ankle Roll", "Left Leg"),

        // Right Arm
        TrimItem(10, "Servo 10 - R Shoulder Pitch", "Right Arm"),
        TrimItem(11, "Servo 11 - R Shoulder Roll", "Right Arm"),
        TrimItem(12, "Servo 12 - R Elbow", "Right Arm"),

        // Left Arm
        TrimItem(13, "Servo 13 - L Shoulder Pitch", "Left Arm"),
        TrimItem(14, "Servo 14 - L Shoulder Roll", "Left Arm"),
        TrimItem(15, "Servo 15 - L Elbow", "Left Arm"),

        // Head & System
        TrimItem(16, "Servo 16 - GPIO 12 Head", "Head & System"),
        TrimItem(17, "Trim 17 - Motion Delay", "Head & System"),
        TrimItem(18, "Trim 18 - PWM Frequency", "Head & System"),
        TrimItem(19, "Trim 19 - Voltage Cal", "Head & System")
    )
}

data class RobotSettingsState(
    val wifiMode: String = "STA",
    val staSsid: String = "",
    val staPass: String = "",
    val apSsid: String = "RoboHero-AP",
    val apPass: String = "",
    val apChannel: Int = 1,
    val dhcpEnabled: Boolean = true,
    val staticIp: String = "192.168.1.100",
    val staticNetmask: String = "255.255.255.0",
    val staticGateway: String = "192.168.1.1",
    val staticDns: String = "192.168.1.1",
    val mqttEnabled: Boolean = false,
    val mqttHost: String = "",
    val mqttPort: Int = 1883,
    val mqttUser: String = "",
    val mqttPass: String = "",
    val mqttClientId: String = "TTR-RoboHero"
)
