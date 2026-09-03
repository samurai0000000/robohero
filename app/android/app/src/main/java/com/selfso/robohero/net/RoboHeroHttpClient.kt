package com.selfso.robohero.net

import android.os.Handler
import android.os.Looper
import okhttp3.Call
import okhttp3.Callback
import okhttp3.HttpUrl
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.TimeUnit

class RoboHeroHttpClient(
    private var baseHost: String = "192.168.4.1",
    private var basePort: Int = 80
) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private val client = OkHttpClient.Builder()
        .connectTimeout(1500, TimeUnit.MILLISECONDS)
        .readTimeout(2000, TimeUnit.MILLISECONDS)
        .writeTimeout(1500, TimeUnit.MILLISECONDS)
        .build()

    fun updateTarget(host: String, port: Int = 80) {
        baseHost = host
        basePort = port
    }

    fun getHost(): String = baseHost

    fun ping(callback: (Boolean) -> Unit) {
        val url = "http://$baseHost:$basePort/?_t=${System.currentTimeMillis()}"
        val req = Request.Builder().url(url).build()
        client.newCall(req).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                mainHandler.post { callback(false) }
            }
            override fun onResponse(call: Call, response: Response) {
                val success = response.isSuccessful
                response.close()
                mainHandler.post { callback(success) }
            }
        })
    }

    fun sendMotion(key: String, value: Int, callback: (Boolean, String) -> Unit) {
        val url = "http://$baseHost:$basePort/?$key=$value&_t=${System.currentTimeMillis()}"
        val req = Request.Builder().url(url).build()
        client.newCall(req).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                mainHandler.post { callback(false, "Connection error") }
            }

            override fun onResponse(call: Call, response: Response) {
                val body = response.body?.string() ?: ""
                val code = response.code
                var msg = "Command sent"

                if (code == 503) {
                    msg = try {
                        val json = JSONObject(body)
                        if (json.optString("status") == "voltage_low") "Voltage low" else "Busy"
                    } catch (e: Exception) {
                        "Robot busy"
                    }
                } else if (response.isSuccessful) {
                    try {
                        val json = JSONObject(body)
                        if (json.optBoolean("stopped", false)) {
                            msg = "Stopped"
                        } else if (json.optBoolean("relaxed", false)) {
                            msg = "Motors relaxed"
                        }
                    } catch (_: Exception) {}
                }

                mainHandler.post {
                    callback(response.isSuccessful, msg)
                }
            }
        })
    }

    fun getTrims(callback: (List<Int>?) -> Unit) {
        val url = "http://$baseHost:$basePort/calibrate?json=1&_t=${System.currentTimeMillis()}"
        val req = Request.Builder().url(url).build()
        client.newCall(req).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                mainHandler.post { callback(null) }
            }

            override fun onResponse(call: Call, response: Response) {
                if (!response.isSuccessful) {
                    mainHandler.post { callback(null) }
                    return
                }
                val body = response.body?.string() ?: ""
                try {
                    val json = JSONObject(body)
                    val arr = json.getJSONArray("trims")
                    val list = mutableListOf<Int>()
                    for (i in 0 until arr.length()) {
                        list.add(arr.getInt(i))
                    }
                    mainHandler.post { callback(list) }
                } catch (e: Exception) {
                    mainHandler.post { callback(null) }
                }
            }
        })
    }

    fun applyTrim(key: Int, value: Int, callback: (Boolean) -> Unit) {
        val url = "http://$baseHost:$basePort/calibrate?apply=1&key=$key&val=$value&_t=${System.currentTimeMillis()}"
        val req = Request.Builder().url(url).build()
        client.newCall(req).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                mainHandler.post { callback(false) }
            }

            override fun onResponse(call: Call, response: Response) {
                val success = response.isSuccessful
                response.close()
                mainHandler.post { callback(success) }
            }
        })
    }

    fun saveTrims(trims: List<Int>, callback: (Boolean, String) -> Unit) {
        val urlBuilder = HttpUrl.Builder()
            .scheme("http")
            .host(baseHost)
            .port(basePort)
            .addPathSegment("calibrate")
            .addQueryParameter("save", "1")

        trims.forEachIndexed { index, value ->
            urlBuilder.addQueryParameter("t$index", value.toString())
        }

        val req = Request.Builder().url(urlBuilder.build()).build()
        client.newCall(req).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                mainHandler.post { callback(false, "Save failed: Network error") }
            }

            override fun onResponse(call: Call, response: Response) {
                val body = response.body?.string() ?: ""
                var msg = "Saved to EEPROM!"
                try {
                    val json = JSONObject(body)
                    msg = json.optString("msg", msg)
                } catch (_: Exception) {}

                mainHandler.post {
                    callback(response.isSuccessful, msg)
                }
            }
        })
    }

    fun sendPose(pose: String, callback: (Boolean) -> Unit) {
        val url = "http://$baseHost:$basePort/calibrate?pose=$pose&_t=${System.currentTimeMillis()}"
        val req = Request.Builder().url(url).build()
        client.newCall(req).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                mainHandler.post { callback(false) }
            }

            override fun onResponse(call: Call, response: Response) {
                val success = response.isSuccessful
                response.close()
                mainHandler.post { callback(success) }
            }
        })
    }
}
