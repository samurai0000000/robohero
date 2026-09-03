package com.selfso.robohero.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.selfso.robohero.data.RobotSettingsState
import com.selfso.robohero.ui.components.RoboHeroButton
import com.selfso.robohero.ui.components.RoboHeroButtonType
import com.selfso.robohero.ui.components.SectionCard
import com.selfso.robohero.ui.theme.*

@Composable
fun SettingsScreen(
    settings: RobotSettingsState = RobotSettingsState(),
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()

    // Form states (disabled for now as per user instruction)
    var wifiMode by remember { mutableStateOf(settings.wifiMode) }
    var staSsid by remember { mutableStateOf(settings.staSsid) }
    var staPass by remember { mutableStateOf(settings.staPass) }
    var apSsid by remember { mutableStateOf(settings.apSsid) }
    var apPass by remember { mutableStateOf(settings.apPass) }
    var apChannel by remember { mutableStateOf(settings.apChannel.toString()) }

    var dhcpEnabled by remember { mutableStateOf(settings.dhcpEnabled) }
    var staticIp by remember { mutableStateOf(settings.staticIp) }
    var staticMask by remember { mutableStateOf(settings.staticNetmask) }
    var staticGw by remember { mutableStateOf(settings.staticGateway) }
    var staticDns by remember { mutableStateOf(settings.staticDns) }

    var mqttEnabled by remember { mutableStateOf(settings.mqttEnabled) }
    var mqttHost by remember { mutableStateOf(settings.mqttHost) }
    var mqttPort by remember { mutableStateOf(settings.mqttPort.toString()) }
    var mqttUser by remember { mutableStateOf(settings.mqttUser) }
    var mqttPass by remember { mutableStateOf(settings.mqttPass) }
    var mqttCid by remember { mutableStateOf(settings.mqttClientId) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // INFO / DISABLED BANNER
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(12.dp))
                .background(Color(0x33F59E0B))
                .border(1.dp, AccentAmber.copy(alpha = 0.6f), RoundedCornerShape(12.dp))
                .padding(14.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Lock,
                    contentDescription = "Protocol pending",
                    tint = AccentAmber
                )
                Column {
                    Text(
                        text = "Firmware Protocol Pending",
                        color = Color(0xFFFDE68A),
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "Serial console settings are exported and viewable below, but editing is disabled until the next firmware protocol update.",
                        color = TextMuted,
                        fontSize = 12.sp
                    )
                }
            }
        }

        // WI-FI SETTINGS CARD (Serial 'wifi' command)
        SectionCard(title = "Wi-Fi Configuration (Serial: wifi)") {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedTextField(
                    value = wifiMode,
                    onValueChange = { wifiMode = it },
                    label = { Text("Wi-Fi Mode (STA / AP / AP-STA / OFF)") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = staSsid,
                    onValueChange = { staSsid = it },
                    label = { Text("Station SSID") },
                    placeholder = { Text("Home Wi-Fi Network") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = staPass,
                    onValueChange = { staPass = it },
                    label = { Text("Station Password") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = apSsid,
                    onValueChange = { apSsid = it },
                    label = { Text("SoftAP SSID") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = apPass,
                    onValueChange = { apPass = it },
                    label = { Text("SoftAP Password") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = apChannel,
                    onValueChange = { apChannel = it },
                    label = { Text("SoftAP Channel (1–13)") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }

        // NETWORK / IP SETTINGS CARD (Serial 'net' command)
        SectionCard(title = "Network & IP (Serial: net)") {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("DHCP Client", color = TextMain, fontSize = 14.sp)
                    Switch(
                        checked = dhcpEnabled,
                        onCheckedChange = { dhcpEnabled = it },
                        enabled = false
                    )
                }

                OutlinedTextField(
                    value = staticIp,
                    onValueChange = { staticIp = it },
                    label = { Text("Static IP Address") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = staticMask,
                    onValueChange = { staticMask = it },
                    label = { Text("Subnet Mask") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = staticGw,
                    onValueChange = { staticGw = it },
                    label = { Text("Default Gateway") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = staticDns,
                    onValueChange = { staticDns = it },
                    label = { Text("DNS Server") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }

        // MQTT BROKER SETTINGS CARD (Serial 'mqtt' command)
        SectionCard(title = "MQTT Broker (Serial: mqtt)") {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("Enable MQTT Client", color = TextMain, fontSize = 14.sp)
                    Switch(
                        checked = mqttEnabled,
                        onCheckedChange = { mqttEnabled = it },
                        enabled = false
                    )
                }

                OutlinedTextField(
                    value = mqttHost,
                    onValueChange = { mqttHost = it },
                    label = { Text("Broker Hostname / IP") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = mqttPort,
                    onValueChange = { mqttPort = it },
                    label = { Text("Broker Port (default 1883)") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = mqttUser,
                    onValueChange = { mqttUser = it },
                    label = { Text("Username") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = mqttPass,
                    onValueChange = { mqttPass = it },
                    label = { Text("Password") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = mqttCid,
                    onValueChange = { mqttCid = it },
                    label = { Text("Client ID (auto: TTR-xxxx)") },
                    enabled = false,
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }

        // EEPROM MAINTENANCE CARD (Serial 'eeprom' command)
        SectionCard(title = "EEPROM Maintenance (Serial: eeprom)") {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                RoboHeroButton(
                    text = "Reset Motion Trims",
                    type = RoboHeroButtonType.OUTLINE,
                    enabled = false,
                    modifier = Modifier.weight(1f),
                    onClick = {}
                )

                RoboHeroButton(
                    text = "Factory Reset",
                    type = RoboHeroButtonType.STOP,
                    enabled = false,
                    modifier = Modifier.weight(1f),
                    onClick = {}
                )
            }
        }

        // STAGING BUTTONS (APPLY & CANCEL)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 10.dp, bottom = 24.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            RoboHeroButton(
                text = "Cancel",
                type = RoboHeroButtonType.RELAX,
                enabled = false,
                modifier = Modifier.weight(1f),
                onClick = {}
            )

            RoboHeroButton(
                text = "Apply",
                type = RoboHeroButtonType.SUCCESS,
                enabled = false,
                modifier = Modifier.weight(1f),
                onClick = {}
            )
        }
    }
}
