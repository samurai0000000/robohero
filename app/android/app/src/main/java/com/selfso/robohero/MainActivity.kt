package com.selfso.robohero

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.*
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material.icons.filled.WifiOff
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.selfso.robohero.data.*
import com.selfso.robohero.net.DeviceDiscoveryManager
import com.selfso.robohero.net.MqttManager
import com.selfso.robohero.net.RoboHeroHttpClient
import com.selfso.robohero.ui.components.ConnectionDialog
import com.selfso.robohero.ui.screens.CalibrationScreen
import com.selfso.robohero.ui.screens.ControllerScreen
import com.selfso.robohero.ui.screens.SettingsScreen
import com.selfso.robohero.ui.theme.*
import kotlinx.coroutines.delay

enum class Screen {
    CONTROLLER,
    CALIBRATION,
    SETTINGS
}

class MainActivity : ComponentActivity() {

    private lateinit var prefs: PreferencesManager
    private lateinit var httpClient: RoboHeroHttpClient
    private lateinit var discoveryManager: DeviceDiscoveryManager
    private lateinit var mqttManager: MqttManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        prefs = PreferencesManager(this)
        httpClient = RoboHeroHttpClient(prefs.robotIp, prefs.robotPort)

        setContent {
            RoboHeroTheme {
                MainAppScreen()
            }
        }
    }

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    private fun MainAppScreen() {
        var currentScreen by remember { mutableStateOf(Screen.CONTROLLER) }
        var menuExpanded by remember { mutableStateOf(false) }
        var showConnectionDialog by remember { mutableStateOf(false) }

        var connectionStatus by remember { mutableStateOf(ConnectionStatus.SEARCHING) }
        var statusLabel by remember { mutableStateOf("Connecting...") }
        var isScanning by remember { mutableStateOf(false) }
        var scanMsg by remember { mutableStateOf("Idle") }

        var toastText by remember { mutableStateOf<String?>(null) }
        val trimsState = remember { mutableStateListOf<TrimItem>().apply { addAll(TrimDefinitions.defaultTrims()) } }

        fun showToast(msg: String) {
            toastText = msg
        }

        // Initialize networking
        LaunchedEffect(Unit) {
            // Ping cached IP first
            httpClient.ping { alive ->
                if (alive) {
                    connectionStatus = ConnectionStatus.CONNECTED_HTTP
                    statusLabel = prefs.robotIp
                } else {
                    connectionStatus = ConnectionStatus.DISCONNECTED
                    statusLabel = "Offline"
                }
            }

            // Setup discovery
            discoveryManager = DeviceDiscoveryManager(
                context = this@MainActivity,
                onDeviceDiscovered = { ip, port, name ->
                    prefs.robotIp = ip
                    prefs.robotPort = port
                    httpClient.updateTarget(ip, port)
                    connectionStatus = ConnectionStatus.CONNECTED_HTTP
                    statusLabel = ip
                    showToast("Found $name ($ip)")
                },
                onStatusChanged = { scanning, msg ->
                    isScanning = scanning
                    scanMsg = msg
                }
            )

            // Setup MQTT if enabled
            mqttManager = MqttManager(
                onConnectionChanged = { connected, info ->
                    if (connected) {
                        connectionStatus = ConnectionStatus.CONNECTED_MQTT
                        statusLabel = "MQTT: ${prefs.mqttHost}"
                    }
                },
                onMessageReceived = { topic, message ->
                    // Handle incoming status messages
                }
            )

            if (prefs.mqttEnabled && prefs.mqttHost.isNotBlank()) {
                mqttManager.connect(
                    host = prefs.mqttHost,
                    port = prefs.mqttPort,
                    user = prefs.mqttUser,
                    pass = prefs.mqttPass,
                    clientId = prefs.mqttClientId
                )
            }
        }

        // Auto-dismiss toast after 1.5s
        LaunchedEffect(toastText) {
            if (toastText != null) {
                delay(1500)
                toastText = null
            }
        }

        // Load trims when calibration screen opens
        LaunchedEffect(currentScreen) {
            if (currentScreen == Screen.CALIBRATION) {
                httpClient.getTrims { list ->
                    if (list != null && list.isNotEmpty()) {
                        list.forEachIndexed { idx, v ->
                            if (idx < trimsState.size) {
                                trimsState[idx] = trimsState[idx].copy(value = v)
                            }
                        }
                    }
                }
            }
        }

        Scaffold(
            topBar = {
                TopAppBar(
                    title = {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Text(
                                text = when (currentScreen) {
                                    Screen.CONTROLLER -> "RoboHero"
                                    Screen.CALIBRATION -> "PWM Calibration"
                                    Screen.SETTINGS -> "Settings"
                                },
                                fontWeight = FontWeight.Bold,
                                fontSize = 18.sp,
                                color = TextMain
                            )
                        }
                    },
                    navigationIcon = {
                        if (currentScreen != Screen.CONTROLLER) {
                            IconButton(onClick = { currentScreen = Screen.CONTROLLER }) {
                                Icon(
                                    imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                                    contentDescription = "Back",
                                    tint = TextMain
                                )
                            }
                        }
                    },
                    actions = {
                        // Connection Status Chip
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(8.dp))
                                .background(
                                    when (connectionStatus) {
                                        ConnectionStatus.CONNECTED_HTTP, ConnectionStatus.CONNECTED_MQTT -> Color(0x3310B981)
                                        ConnectionStatus.SEARCHING -> Color(0x3306B6D4)
                                        else -> Color(0x33DC2626)
                                    }
                                )
                                .border(
                                    1.dp,
                                    when (connectionStatus) {
                                        ConnectionStatus.CONNECTED_HTTP, ConnectionStatus.CONNECTED_MQTT -> AccentGreen
                                        ConnectionStatus.SEARCHING -> AccentCyan
                                        else -> AccentRed
                                    },
                                    RoundedCornerShape(8.dp)
                                )
                                .clickable { showConnectionDialog = true }
                                .padding(horizontal = 8.dp, vertical = 4.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Icon(
                                    imageVector = if (connectionStatus == ConnectionStatus.DISCONNECTED) Icons.Default.WifiOff else Icons.Default.Wifi,
                                    contentDescription = "Connection",
                                    tint = when (connectionStatus) {
                                        ConnectionStatus.CONNECTED_HTTP, ConnectionStatus.CONNECTED_MQTT -> AccentGreen
                                        ConnectionStatus.SEARCHING -> AccentCyan
                                        else -> AccentRed
                                    },
                                    modifier = Modifier.size(14.dp)
                                )
                                Text(
                                    text = statusLabel,
                                    fontSize = 11.sp,
                                    color = TextMain,
                                    fontWeight = FontWeight.SemiBold
                                )
                            }
                        }

                        // Top-Right Menu Button
                        IconButton(onClick = { menuExpanded = true }) {
                            Icon(
                                imageVector = Icons.Default.MoreVert,
                                contentDescription = "Menu",
                                tint = TextMain
                            )
                        }

                        DropdownMenu(
                            expanded = menuExpanded,
                            onDismissRequest = { menuExpanded = false },
                            modifier = Modifier.background(BgCard)
                        ) {
                            DropdownMenuItem(
                                text = { Text("Settings", color = TextMain, fontWeight = FontWeight.SemiBold) },
                                onClick = {
                                    menuExpanded = false
                                    currentScreen = Screen.SETTINGS
                                }
                            )
                            DropdownMenuItem(
                                text = { Text("PWM calibration", color = TextMain, fontWeight = FontWeight.SemiBold) },
                                onClick = {
                                    menuExpanded = false
                                    currentScreen = Screen.CALIBRATION
                                }
                            )
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = BgCard
                    )
                )
            },
            containerColor = BgPrimary
        ) { innerPadding ->
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding)
            ) {
                when (currentScreen) {
                    Screen.CONTROLLER -> {
                        ControllerScreen(
                            onSendCmd = { key, value ->
                                httpClient.sendMotion(key, value) { success, msg ->
                                    showToast(msg)
                                }
                            }
                        )
                    }
                    Screen.CALIBRATION -> {
                        CalibrationScreen(
                            trims = trimsState,
                            onTrimChanged = { key, value ->
                                val idx = trimsState.indexOfFirst { it.id == key }
                                if (idx != -1) {
                                    trimsState[idx] = trimsState[idx].copy(value = value)
                                }
                                httpClient.applyTrim(key, value) {}
                            },
                            onSaveTrims = {
                                val list = trimsState.map { it.value }
                                httpClient.saveTrims(list) { success, msg ->
                                    showToast(msg)
                                }
                            },
                            onSetPose = { pose ->
                                httpClient.sendPose(pose) { success ->
                                    showToast(if (success) "Pose applied" else "Pose failed")
                                }
                            }
                        )
                    }
                    Screen.SETTINGS -> {
                        SettingsScreen()
                    }
                }

                // Toast Notification Overlay
                AnimatedVisibility(
                    visible = toastText != null,
                    enter = slideInVertically(initialOffsetY = { it }) + fadeIn(),
                    exit = slideOutVertically(targetOffsetY = { it }) + fadeOut(),
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 24.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(Color(0xE610B981))
                            .padding(horizontal = 20.dp, vertical = 10.dp)
                    ) {
                        Text(
                            text = toastText ?: "",
                            color = Color.White,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }

                // Connection Dialog
                if (showConnectionDialog) {
                    ConnectionDialog(
                        currentIp = prefs.robotIp,
                        currentPort = prefs.robotPort,
                        isScanning = isScanning,
                        scanMessage = scanMsg,
                        onStartScan = {
                            discoveryManager.startDiscovery()
                        },
                        onConnectDirectAp = {
                            prefs.robotIp = "192.168.4.1"
                            prefs.robotPort = 80
                            httpClient.updateTarget("192.168.4.1", 80)
                            connectionStatus = ConnectionStatus.CONNECTED_HTTP
                            statusLabel = "192.168.4.1"
                            showToast("Connected to Robot AP")
                        },
                        onSaveManual = { ip, port ->
                            prefs.robotIp = ip
                            prefs.robotPort = port
                            httpClient.updateTarget(ip, port)
                            httpClient.ping { alive ->
                                connectionStatus = if (alive) ConnectionStatus.CONNECTED_HTTP else ConnectionStatus.DISCONNECTED
                                statusLabel = if (alive) ip else "Offline"
                                showToast(if (alive) "Connected to $ip" else "Failed to connect to $ip")
                            }
                        },
                        onDismiss = { showConnectionDialog = false }
                    )
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (::discoveryManager.isInitialized) {
            discoveryManager.stopDiscovery()
        }
        if (::mqttManager.isInitialized) {
            mqttManager.disconnect()
        }
    }
}
