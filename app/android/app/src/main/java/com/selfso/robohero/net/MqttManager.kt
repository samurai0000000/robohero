package com.selfso.robohero.net

import android.os.Handler
import android.os.Looper
import android.util.Log
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken
import org.eclipse.paho.client.mqttv3.MqttCallback
import org.eclipse.paho.client.mqttv3.MqttClient
import org.eclipse.paho.client.mqttv3.MqttConnectOptions
import org.eclipse.paho.client.mqttv3.MqttMessage
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence

class MqttManager(
    private val onConnectionChanged: (connected: Boolean, info: String) -> Unit,
    private val onMessageReceived: (topic: String, message: String) -> Unit
) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private var client: MqttClient? = null
    private var isConnected = false
    private var targetClientId = "robohero"

    fun connect(
        host: String,
        port: Int = 1883,
        user: String = "",
        pass: String = "",
        clientId: String = ""
    ) {
        disconnect()

        if (host.isBlank()) {
            postConnection(false, "Broker host is empty")
            return
        }

        val cid = if (clientId.isNotBlank()) clientId else "RoboHeroApp-${System.currentTimeMillis() % 10000}"
        targetClientId = if (clientId.isNotBlank()) clientId else "robohero"
        val serverUri = "tcp://$host:$port"

        Thread {
            try {
                val persistence = MemoryPersistence()
                val mqttClient = MqttClient(serverUri, cid, persistence)
                val options = MqttConnectOptions().apply {
                    isCleanSession = true
                    connectionTimeout = 5
                    keepAliveInterval = 30
                    if (user.isNotBlank()) {
                        userName = user
                        password = pass.toCharArray()
                    }
                }

                mqttClient.setCallback(object : MqttCallback {
                    override fun connectionLost(cause: Throwable?) {
                        Log.w(TAG, "MQTT connection lost", cause)
                        isConnected = false
                        postConnection(false, "MQTT disconnected")
                    }

                    override fun messageArrived(topic: String?, message: MqttMessage?) {
                        val payload = message?.payload?.let { String(it) } ?: ""
                        topic?.let { t ->
                            mainHandler.post {
                                onMessageReceived(t, payload)
                            }
                        }
                    }

                    override fun deliveryComplete(token: IMqttDeliveryToken?) {}
                })

                mqttClient.connect(options)
                client = mqttClient
                isConnected = true

                // Subscribe to robot status
                val subTopic = "robot/robohero/+/status"
                mqttClient.subscribe(subTopic, 0)

                postConnection(true, "Connected to $host:$port")
            } catch (e: Exception) {
                Log.e(TAG, "MQTT connect failed", e)
                isConnected = false
                postConnection(false, "MQTT error: ${e.message}")
            }
        }.start()
    }

    fun publishCmd(cmdText: String, callback: ((Boolean) -> Unit)? = null) {
        val activeClient = client
        if (!isConnected || activeClient == null) {
            callback?.invoke(false)
            return
        }

        val topic = "robot/robohero/$targetClientId/cmd"
        Thread {
            try {
                val message = MqttMessage(cmdText.toByteArray()).apply {
                    qos = 0
                    isRetained = false
                }
                activeClient.publish(topic, message)
                mainHandler.post { callback?.invoke(true) }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to publish cmd: $cmdText", e)
                mainHandler.post { callback?.invoke(false) }
            }
        }.start()
    }

    fun disconnect() {
        try {
            if (client?.isConnected == true) {
                client?.disconnect()
            }
            client?.close()
        } catch (_: Exception) {}
        client = null
        isConnected = false
        postConnection(false, "Disconnected")
    }

    private fun postConnection(connected: Boolean, info: String) {
        mainHandler.post {
            onConnectionChanged(connected, info)
        }
    }

    companion object {
        private const val TAG = "MqttManager"
    }
}
