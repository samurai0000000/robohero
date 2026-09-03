package com.selfso.robohero.net

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.Handler
import android.os.Looper
import android.util.Log

class DeviceDiscoveryManager(
    context: Context,
    private val onDeviceDiscovered: (ip: String, port: Int, name: String) -> Unit,
    private val onStatusChanged: (isScanning: Boolean, message: String) -> Unit
) {
    private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as? NsdManager
    private val mainHandler = Handler(Looper.getMainLooper())
    private var discoveryListener: NsdManager.DiscoveryListener? = null
    private var isDiscovering = false

    fun startDiscovery() {
        if (isDiscovering || nsdManager == null) return

        discoveryListener = object : NsdManager.DiscoveryListener {
            override fun onStartDiscoveryFailed(serviceType: String?, errorCode: Int) {
                Log.e(TAG, "Discovery start failed: $errorCode")
                isDiscovering = false
                postStatus(false, "mDNS discovery failed")
            }

            override fun onStopDiscoveryFailed(serviceType: String?, errorCode: Int) {
                Log.e(TAG, "Discovery stop failed: $errorCode")
                isDiscovering = false
                postStatus(false, "Discovery stopped")
            }

            override fun onDiscoveryStarted(regType: String?) {
                Log.i(TAG, "Discovery started for $regType")
                isDiscovering = true
                postStatus(true, "Scanning LAN for RoboHero...")
            }

            override fun onDiscoveryStopped(serviceType: String?) {
                Log.i(TAG, "Discovery stopped")
                isDiscovering = false
                postStatus(false, "Discovery idle")
            }

            override fun onServiceFound(serviceInfo: NsdServiceInfo?) {
                if (serviceInfo == null) return
                val name = serviceInfo.serviceName ?: ""
                Log.i(TAG, "Service found: $name, type=${serviceInfo.serviceType}")

                if (name.contains("robohero", ignoreCase = true) ||
                    name.contains("ttr", ignoreCase = true)) {
                    resolveService(serviceInfo)
                }
            }

            override fun onServiceLost(serviceInfo: NsdServiceInfo?) {
                Log.i(TAG, "Service lost: ${serviceInfo?.serviceName}")
            }
        }

        try {
            nsdManager.discoverServices("_http._tcp.", NsdManager.PROTOCOL_DNS_SD, discoveryListener)
        } catch (e: Exception) {
            Log.e(TAG, "Error starting discovery", e)
            postStatus(false, "Discovery error: ${e.message}")
        }
    }

    private fun resolveService(serviceInfo: NsdServiceInfo) {
        nsdManager?.resolveService(serviceInfo, object : NsdManager.ResolveListener {
            override fun onResolveFailed(serviceInfo: NsdServiceInfo?, errorCode: Int) {
                Log.w(TAG, "Resolve failed for ${serviceInfo?.serviceName}: $errorCode")
            }

            override fun onServiceResolved(resolvedInfo: NsdServiceInfo?) {
                if (resolvedInfo == null) return
                val host = resolvedInfo.host?.hostAddress ?: return
                val port = resolvedInfo.port
                val name = resolvedInfo.serviceName ?: "RoboHero"

                Log.i(TAG, "RoboHero resolved: $host:$port ($name)")
                mainHandler.post {
                    onDeviceDiscovered(host, port, name)
                    postStatus(false, "Discovered $name at $host")
                }
            }
        })
    }

    fun stopDiscovery() {
        if (!isDiscovering || nsdManager == null || discoveryListener == null) return
        try {
            nsdManager.stopServiceDiscovery(discoveryListener)
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping discovery", e)
        } finally {
            isDiscovering = false
            discoveryListener = null
        }
    }

    private fun postStatus(scanning: Boolean, msg: String) {
        mainHandler.post {
            onStatusChanged(scanning, msg)
        }
    }

    companion object {
        private const val TAG = "DiscoveryManager"
    }
}
