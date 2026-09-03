package com.selfso.robohero.net

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.net.wifi.WifiManager
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.util.concurrent.atomic.AtomicBoolean

class DeviceDiscoveryManager(
    private val context: Context,
    private val onDeviceDiscovered: (ip: String, port: Int, name: String) -> Unit,
    private val onStatusChanged: (isScanning: Boolean, message: String) -> Unit
) {
    private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as? NsdManager
    private val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
    private var multicastLock: WifiManager.MulticastLock? = null

    private val mainHandler = Handler(Looper.getMainLooper())
    private var discoveryListener: NsdManager.DiscoveryListener? = null
    private var isDiscovering = AtomicBoolean(false)
    private var udpSocket: DatagramSocket? = null
    private var udpThread: Thread? = null

    fun startDiscovery() {
        if (isDiscovering.getAndSet(true)) return

        acquireMulticastLock()
        postStatus(true, "Scanning LAN (mDNS + UDP Broadcast)...")

        startNsdDiscovery()
        startUdpBroadcastDiscovery()
    }

    private fun acquireMulticastLock() {
        try {
            if (multicastLock == null) {
                multicastLock = wifiManager?.createMulticastLock("RoboHeroMulticastLock")?.apply {
                    setReferenceCounted(true)
                }
            }
            multicastLock?.acquire()
        } catch (e: Exception) {
            Log.w(TAG, "Failed to acquire multicast lock", e)
        }
    }

    private fun releaseMulticastLock() {
        try {
            if (multicastLock?.isHeld == true) {
                multicastLock?.release()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to release multicast lock", e)
        }
    }

    private fun startNsdDiscovery() {
        if (nsdManager == null) return

        discoveryListener = object : NsdManager.DiscoveryListener {
            override fun onStartDiscoveryFailed(serviceType: String?, errorCode: Int) {
                Log.e(TAG, "mDNS start failed: $errorCode")
            }

            override fun onStopDiscoveryFailed(serviceType: String?, errorCode: Int) {
                Log.e(TAG, "mDNS stop failed: $errorCode")
            }

            override fun onDiscoveryStarted(regType: String?) {
                Log.i(TAG, "mDNS discovery started for $regType")
            }

            override fun onDiscoveryStopped(serviceType: String?) {
                Log.i(TAG, "mDNS discovery stopped")
            }

            override fun onServiceFound(serviceInfo: NsdServiceInfo?) {
                if (serviceInfo == null || !isDiscovering.get()) return
                val name = serviceInfo.serviceName ?: ""
                Log.i(TAG, "mDNS service found: $name")

                if (name.contains("robohero", ignoreCase = true) ||
                    name.contains("ttr", ignoreCase = true)) {
                    resolveNsdService(serviceInfo)
                }
            }

            override fun onServiceLost(serviceInfo: NsdServiceInfo?) {
                Log.i(TAG, "mDNS service lost: ${serviceInfo?.serviceName}")
            }
        }

        try {
            nsdManager.discoverServices("_http._tcp.", NsdManager.PROTOCOL_DNS_SD, discoveryListener)
        } catch (e: Exception) {
            Log.e(TAG, "Error starting mDNS", e)
        }
    }

    private fun resolveNsdService(serviceInfo: NsdServiceInfo) {
        nsdManager?.resolveService(serviceInfo, object : NsdManager.ResolveListener {
            override fun onResolveFailed(serviceInfo: NsdServiceInfo?, errorCode: Int) {
                Log.w(TAG, "mDNS resolve failed: $errorCode")
            }

            override fun onServiceResolved(resolvedInfo: NsdServiceInfo?) {
                if (resolvedInfo == null || !isDiscovering.get()) return
                val host = resolvedInfo.host?.hostAddress ?: return
                val port = resolvedInfo.port
                val name = resolvedInfo.serviceName ?: "RoboHero"

                Log.i(TAG, "mDNS resolved: $host:$port ($name)")
                handleDeviceFound(host, port, name, "mDNS")
            }
        })
    }

    private fun startUdpBroadcastDiscovery() {
        udpThread = Thread {
            var socket: DatagramSocket? = null
            try {
                socket = DatagramSocket().apply {
                    broadcast = true
                    soTimeout = 1500
                }
                udpSocket = socket

                val discoverBytes = "ROBOHERO_DISCOVER".toByteArray()
                val broadcastAddr = InetAddress.getByName("255.255.255.255")
                val sendPacket = DatagramPacket(discoverBytes, discoverBytes.size, broadcastAddr, UDP_PORT)

                val buffer = ByteArray(512)
                val receivePacket = DatagramPacket(buffer, buffer.size)

                var attempts = 0
                while (isDiscovering.get() && attempts < 10) {
                    attempts++
                    try {
                        socket.send(sendPacket)
                        Log.d(TAG, "Sent UDP broadcast discover attempt #$attempts")
                    } catch (e: Exception) {
                        Log.w(TAG, "Failed to send UDP broadcast", e)
                    }

                    val endTime = System.currentTimeMillis() + 1500
                    while (isDiscovering.get() && System.currentTimeMillis() < endTime) {
                        try {
                            socket.receive(receivePacket)
                            val reply = String(receivePacket.data, 0, receivePacket.length).trim()
                            val senderIp = receivePacket.address.hostAddress ?: ""
                            Log.i(TAG, "UDP reply from $senderIp: $reply")

                            if (reply.startsWith("ROBOHERO_HERE")) {
                                // Format: ROBOHERO_HERE or ROBOHERO_HERE:<port> or ROBOHERO_HERE:<port>:<name>
                                val parts = reply.split(":")
                                val port = if (parts.size > 1) parts[1].toIntOrNull() ?: 80 else 80
                                val name = if (parts.size > 2) parts[2] else "RoboHero"

                                handleDeviceFound(senderIp, port, name, "UDP")
                                return@Thread
                            }
                        } catch (_: java.net.SocketTimeoutException) {
                            // Timeout per packet read, continue
                        } catch (e: Exception) {
                            if (isDiscovering.get()) {
                                Log.w(TAG, "Error in UDP receive", e)
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in UDP discovery loop", e)
            } finally {
                try {
                    socket?.close()
                } catch (_: Exception) {}
            }
        }.apply { start() }
    }

    private fun handleDeviceFound(host: String, port: Int, name: String, method: String) {
        if (!isDiscovering.getAndSet(false)) return
        Log.i(TAG, "Found device via $method: $host:$port ($name)")

        mainHandler.post {
            onDeviceDiscovered(host, port, name)
            postStatus(false, "Found $name via $method ($host)")
        }

        stopDiscovery()
    }

    fun stopDiscovery() {
        isDiscovering.set(false)
        releaseMulticastLock()

        // Stop mDNS
        if (nsdManager != null && discoveryListener != null) {
            try {
                nsdManager.stopServiceDiscovery(discoveryListener)
            } catch (e: Exception) {
                Log.d(TAG, "Error stopping mDNS: ${e.message}")
            }
            discoveryListener = null
        }

        // Stop UDP
        try {
            udpSocket?.close()
        } catch (_: Exception) {}
        udpSocket = null
        udpThread = null

        postStatus(false, "Discovery idle")
    }

    private fun postStatus(scanning: Boolean, msg: String) {
        mainHandler.post {
            onStatusChanged(scanning, msg)
        }
    }

    companion object {
        private const val TAG = "DiscoveryManager"
        const val UDP_PORT = 8266
    }
}
