package com.selfso.robohero.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.selfso.robohero.ui.components.RoboHeroButton
import com.selfso.robohero.ui.components.RoboHeroButtonType
import com.selfso.robohero.ui.components.SectionCard

@Composable
fun ControllerScreen(
    onSendCmd: (key: String, value: Int) -> Unit
) {
    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // LOCOMOTION CARD (3x3 D-Pad)
        SectionCard(title = "Locomotion") {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                // Row 1
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Turn Left",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.PRIMARY,
                        onClick = { onSendCmd("pm", 3) }
                    )
                    RoboHeroButton(
                        text = "Forward",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.PRIMARY,
                        onClick = { onSendCmd("pm", 1) }
                    )
                    RoboHeroButton(
                        text = "Turn Right",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.PRIMARY,
                        onClick = { onSendCmd("pm", 4) }
                    )
                }

                // Row 2
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Move Left",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.PRIMARY,
                        onClick = { onSendCmd("pm", 5) }
                    )
                    RoboHeroButton(
                        text = "STANDBY",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.STANDBY,
                        onClick = { onSendCmd("pm", 99) }
                    )
                    RoboHeroButton(
                        text = "Move Right",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.PRIMARY,
                        onClick = { onSendCmd("pm", 6) }
                    )
                }

                // Row 3
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Relax",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.RELAX,
                        onClick = { onSendCmd("relax", 1) }
                    )
                    RoboHeroButton(
                        text = "Backward",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.PRIMARY,
                        onClick = { onSendCmd("pm", 2) }
                    )
                    Spacer(modifier = Modifier.weight(1f))
                }
            }
        }

        // RECOVERY CARD
        SectionCard(title = "Recovery") {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                RoboHeroButton(
                    text = "Get Up",
                    modifier = Modifier.weight(1f),
                    type = RoboHeroButtonType.PRIMARY,
                    onClick = { onSendCmd("pm", 11) }
                )
                RoboHeroButton(
                    text = "Face-Down Get Up",
                    modifier = Modifier.weight(1f),
                    type = RoboHeroButtonType.PRIMARY,
                    onClick = { onSendCmd("pm", 12) }
                )
            }
        }

        // ACTIONS & GESTURES CARD
        SectionCard(title = "Actions & Gestures") {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Bow",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 1) }
                    )
                    RoboHeroButton(
                        text = "Apache",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 4) }
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Waving",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 2) }
                    )
                    RoboHeroButton(
                        text = "Balance",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 5) }
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Iron Man",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 3) }
                    )
                    RoboHeroButton(
                        text = "Warm-Up",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 6) }
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Clap Hands",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 7) }
                    )
                    RoboHeroButton(
                        text = "GOILC",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 8) }
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    RoboHeroButton(
                        text = "Dance",
                        modifier = Modifier.weight(1f),
                        type = RoboHeroButtonType.ACTION,
                        onClick = { onSendCmd("pms", 9) }
                    )
                    Spacer(modifier = Modifier.weight(1f))
                }

                // Full-width buttons
                RoboHeroButton(
                    text = "Auto Demo Loop",
                    modifier = Modifier.fillMaxWidth(),
                    type = RoboHeroButtonType.AUTO,
                    onClick = { onSendCmd("pms", 99) }
                )

                RoboHeroButton(
                    text = "STOP",
                    modifier = Modifier.fillMaxWidth(),
                    type = RoboHeroButtonType.STOP,
                    onClick = { onSendCmd("stop", 1) }
                )
            }
        }

        Spacer(modifier = Modifier.height(24.dp))
    }
}
