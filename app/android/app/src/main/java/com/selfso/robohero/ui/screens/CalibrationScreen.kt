package com.selfso.robohero.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.selfso.robohero.data.TrimItem
import com.selfso.robohero.ui.components.RoboHeroButton
import com.selfso.robohero.ui.components.RoboHeroButtonType
import com.selfso.robohero.ui.components.SectionCard
import com.selfso.robohero.ui.theme.*

@Composable
fun CalibrationScreen(
    trims: List<TrimItem>,
    isConnected: Boolean = true,
    onTrimChanged: (key: Int, value: Int) -> Unit,
    onSaveTrims: () -> Unit,
    onSetPose: (pose: String) -> Unit,
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Quick Actions Card
        SectionCard(title = "Actions & Save") {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                RoboHeroButton(
                    text = "Zero Pose",
                    modifier = Modifier.weight(1f),
                    type = RoboHeroButtonType.OUTLINE,
                    enabled = isConnected,
                    onClick = { onSetPose("zero") }
                )
                RoboHeroButton(
                    text = "Center Pose",
                    modifier = Modifier.weight(1f),
                    type = RoboHeroButtonType.OUTLINE,
                    enabled = isConnected,
                    onClick = { onSetPose("center") }
                )
                RoboHeroButton(
                    text = "Save EEPROM",
                    modifier = Modifier.weight(1.3f),
                    type = RoboHeroButtonType.SUCCESS,
                    enabled = isConnected,
                    onClick = onSaveTrims
                )
            }
        }

        val categories = listOf("Right Leg", "Left Leg", "Right Arm", "Left Arm", "Head & System")

        categories.forEach { cat ->
            val catTrims = trims.filter { it.category == cat }
            if (catTrims.isNotEmpty()) {
                SectionCard(title = cat) {
                    Column(verticalArrangement = Arrangement.spacedBy(14.dp)) {
                        catTrims.forEach { item ->
                            TrimRow(
                                item = item,
                                enabled = isConnected,
                                onValueChange = { newVal ->
                                    onTrimChanged(item.id, newVal)
                                }
                            )
                        }
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(30.dp))
    }
}

@Composable
private fun TrimRow(
    item: TrimItem,
    enabled: Boolean = true,
    onValueChange: (Int) -> Unit
) {
    val shape = RoundedCornerShape(8.dp)
    val alpha = if (enabled) 1f else 0.45f

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(shape)
            .background(Color(0x22000000))
            .border(1.dp, BorderSubtle.copy(alpha = alpha), shape)
            .padding(10.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = item.name,
                color = TextMain.copy(alpha = alpha),
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold
            )
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(6.dp))
                    .background(Color(0x3306B6D4).copy(alpha = alpha))
                    .border(1.dp, AccentCyan.copy(alpha = 0.5f * alpha), RoundedCornerShape(6.dp))
                    .padding(horizontal = 8.dp, vertical = 2.dp)
            ) {
                Text(
                    text = "${item.value}",
                    color = AccentCyan.copy(alpha = alpha),
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // Minus button
            Box(
                modifier = Modifier
                    .size(36.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(BgCard)
                    .border(1.dp, BorderSubtle, RoundedCornerShape(6.dp)),
                contentAlignment = Alignment.Center
            ) {
                RoboHeroButton(
                    text = "-",
                    type = RoboHeroButtonType.OUTLINE,
                    enabled = enabled,
                    modifier = Modifier.fillMaxSize(),
                    onClick = {
                        val v = (item.value - 1).coerceIn(item.min, item.max)
                        onValueChange(v)
                    }
                )
            }

            Slider(
                value = item.value.toFloat(),
                onValueChange = { onValueChange(it.toInt()) },
                valueRange = item.min.toFloat()..item.max.toFloat(),
                enabled = enabled,
                colors = SliderDefaults.colors(
                    thumbColor = if (enabled) AccentCyan else Color.Gray,
                    activeTrackColor = if (enabled) AccentBlue else Color.DarkGray,
                    inactiveTrackColor = Color(0x33FFFFFF)
                ),
                modifier = Modifier.weight(1f)
            )

            // Plus button
            Box(
                modifier = Modifier
                    .size(36.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(BgCard)
                    .border(1.dp, BorderSubtle, RoundedCornerShape(6.dp)),
                contentAlignment = Alignment.Center
            ) {
                RoboHeroButton(
                    text = "+",
                    type = RoboHeroButtonType.OUTLINE,
                    enabled = enabled,
                    modifier = Modifier.fillMaxSize(),
                    onClick = {
                        val v = (item.value + 1).coerceIn(item.min, item.max)
                        onValueChange(v)
                    }
                )
            }

            // Zero button
            Box(
                modifier = Modifier
                    .size(36.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(BgCard)
                    .border(1.dp, BorderSubtle, RoundedCornerShape(6.dp)),
                contentAlignment = Alignment.Center
            ) {
                RoboHeroButton(
                    text = "0",
                    type = RoboHeroButtonType.OUTLINE,
                    enabled = enabled,
                    modifier = Modifier.fillMaxSize(),
                    onClick = { onValueChange(0) }
                )
            }
        }
    }
}
