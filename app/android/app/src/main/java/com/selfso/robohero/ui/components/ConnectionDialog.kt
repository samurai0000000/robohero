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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.selfso.robohero.ui.theme.*

@Composable
fun ConnectionDialog(
    currentIp: String,
    currentPort: Int,
    isScanning: Boolean,
    scanMessage: String,
    onStartScan: () -> Unit,
    onConnectDirectAp: () -> Unit,
    onSaveManual: (ip: String, port: Int) -> Unit,
    onDismiss: () -> Unit
) {
    var ipText by remember { mutableStateOf(currentIp) }
    var portText by remember { mutableStateOf(currentPort.toString()) }
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
                text = if (isScanning) "Status: Scanning LAN..." else "Status: $scanMessage",
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
