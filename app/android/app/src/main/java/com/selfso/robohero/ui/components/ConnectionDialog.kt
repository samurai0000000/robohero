package com.selfso.robohero.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.selfso.robohero.ui.theme.*

data class DiscoveredCandidate(
    val ip: String,
    val port: Int,
    val name: String
)

@Composable
fun ConnectionDialog(
    currentIp: String,
    currentPort: Int,
    isScanning: Boolean,
    scanMessage: String,
    candidateDevice: DiscoveredCandidate? = null,
    onStartScan: () -> Unit,
    onAcceptCandidate: (DiscoveredCandidate) -> Unit,
    onRejectCandidate: () -> Unit,
    onConnectDirectAp: () -> Unit,
    onSaveManual: (ip: String, port: Int) -> Unit,
    onDismiss: () -> Unit
) {
    var ipText by remember(currentIp) { mutableStateOf(currentIp) }
    var portText by remember(currentPort) { mutableStateOf(if (currentPort > 0) currentPort.toString() else "80") }
    val shape = RoundedCornerShape(16.dp)

    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(shape)
                .background(BgPrimary)
                .border(1.dp, BorderSubtle, shape)
                .padding(20.dp)
        ) {
            Text(
                text = "Robot Connection",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                color = TextMain
            )

            Spacer(modifier = Modifier.height(14.dp))

            // Discovery status
            Text(
                text = if (isScanning) "Status: Scanning LAN (mDNS + UDP)..." else "Status: $scanMessage",
                fontSize = 12.sp,
                color = if (isScanning) AccentCyan else TextMuted
            )

            Spacer(modifier = Modifier.height(10.dp))

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                RoboHeroButton(
                    text = if (isScanning) "Scanning..." else "Scan via mDNS",
                    type = RoboHeroButtonType.ACTION,
                    enabled = !isScanning,
                    modifier = Modifier.weight(1f),
                    onClick = onStartScan
                )

                RoboHeroButton(
                    text = "Direct AP (192.168.4.1)",
                    type = RoboHeroButtonType.PRIMARY,
                    modifier = Modifier.weight(1.3f),
                    onClick = {
                        ipText = "192.168.4.1"
                        portText = "80"
                        onConnectDirectAp()
                    }
                )
            }

            // Candidate confirmation banner
            if (candidateDevice != null) {
                Spacer(modifier = Modifier.height(12.dp))
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(10.dp))
                        .background(Color(0x3310B981))
                        .border(1.dp, AccentGreen.copy(alpha = 0.6f), RoundedCornerShape(10.dp))
                        .padding(12.dp)
                ) {
                    Text(
                        text = "Candidate Found: ${candidateDevice.name}",
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        color = AccentGreen
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        text = "IP: ${candidateDevice.ip}:${candidateDevice.port}\nAccept and connect to this address?",
                        fontSize = 12.sp,
                        color = TextMain
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        OutlinedButton(
                            onClick = onRejectCandidate,
                            modifier = Modifier.weight(1f),
                            shape = RoundedCornerShape(8.dp),
                            border = androidx.compose.foundation.BorderStroke(1.dp, BorderSubtle)
                        ) {
                            Text("Reject", color = TextMuted, fontSize = 12.sp)
                        }
                        Button(
                            onClick = {
                                ipText = candidateDevice.ip
                                portText = candidateDevice.port.toString()
                                onAcceptCandidate(candidateDevice)
                            },
                            modifier = Modifier.weight(1f),
                            shape = RoundedCornerShape(8.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF10B981))
                        ) {
                            Text("Accept", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 12.sp)
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(16.dp))
            HorizontalDivider(color = BorderSubtle)
            Spacer(modifier = Modifier.height(16.dp))

            Text(
                text = "MANUAL IP OVERRIDE",
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                color = TextMuted
            )

            Spacer(modifier = Modifier.height(8.dp))

            OutlinedTextField(
                value = ipText,
                onValueChange = { ipText = it },
                label = { Text("Robot IP Address") },
                placeholder = { Text("e.g. 192.168.1.100") },
                singleLine = true,
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = TextMain,
                    unfocusedTextColor = TextMain,
                    focusedBorderColor = AccentCyan,
                    unfocusedBorderColor = BorderSubtle,
                    focusedLabelColor = AccentCyan,
                    unfocusedLabelColor = TextMuted
                ),
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(8.dp))

            OutlinedTextField(
                value = portText,
                onValueChange = { portText = it },
                label = { Text("Port (default 80)") },
                singleLine = true,
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = TextMain,
                    unfocusedTextColor = TextMain,
                    focusedBorderColor = AccentCyan,
                    unfocusedBorderColor = BorderSubtle,
                    focusedLabelColor = AccentCyan,
                    unfocusedLabelColor = TextMuted
                ),
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(20.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.End
            ) {
                TextButton(onClick = onDismiss) {
                    Text("Close", color = TextMuted)
                }
                Spacer(modifier = Modifier.width(8.dp))
                Button(
                    onClick = {
                        val port = portText.toIntOrNull() ?: 80
                        onSaveManual(ipText.trim(), port)
                        onDismiss()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = AccentBlue)
                ) {
                    Text("Connect", color = TextMain, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}
