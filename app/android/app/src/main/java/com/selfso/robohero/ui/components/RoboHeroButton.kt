package com.selfso.robohero.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.selfso.robohero.ui.theme.*

enum class RoboHeroButtonType {
    PRIMARY,
    STANDBY,
    RELAX,
    ACTION,
    AUTO,
    STOP,
    SUCCESS,
    OUTLINE
}

@Composable
fun RoboHeroButton(
    text: String,
    modifier: Modifier = Modifier,
    type: RoboHeroButtonType = RoboHeroButtonType.PRIMARY,
    enabled: Boolean = true,
    onClick: () -> Unit
) {
    val shape = RoundedCornerShape(10.dp)
    val alpha = if (enabled) 1.0f else 0.45f

    val backgroundModifier = when (type) {
        RoboHeroButtonType.PRIMARY -> Modifier.background(
            Brush.linearGradient(listOf(PrimaryGradientStart, PrimaryGradientEnd))
        )
        RoboHeroButtonType.STANDBY -> Modifier.background(
            Brush.linearGradient(listOf(StandbyGradientStart, StandbyGradientEnd))
        )
        RoboHeroButtonType.RELAX -> Modifier.background(
            Brush.linearGradient(listOf(RelaxGradientStart, RelaxGradientEnd))
        )
        RoboHeroButtonType.ACTION -> Modifier
            .background(Color(0x33F59E0B))
            .border(1.dp, AccentAmber.copy(alpha = 0.8f * alpha), shape)
        RoboHeroButtonType.AUTO -> Modifier.background(
            Brush.linearGradient(listOf(AutoGradientStart, AutoGradientEnd))
        )
        RoboHeroButtonType.STOP -> Modifier.background(
            Brush.linearGradient(listOf(StopGradientStart, StopGradientEnd))
        )
        RoboHeroButtonType.SUCCESS -> Modifier.background(
            Brush.linearGradient(listOf(Color(0xFF10B981), Color(0xFF059669)))
        )
        RoboHeroButtonType.OUTLINE -> Modifier
            .background(BgCard)
            .border(1.dp, BorderSubtle, shape)
    }

    val textColor = when (type) {
        RoboHeroButtonType.ACTION -> Color(0xFFFDE68A).copy(alpha = alpha)
        else -> TextMain.copy(alpha = alpha)
    }

    val fontWeight = when (type) {
        RoboHeroButtonType.STANDBY, RoboHeroButtonType.STOP, RoboHeroButtonType.AUTO -> FontWeight.Bold
        else -> FontWeight.SemiBold
    }

    Box(
        modifier = modifier
            .heightIn(min = 48.dp)
            .clip(shape)
            .then(backgroundModifier)
            .clickable(
                enabled = enabled,
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = onClick
            )
            .padding(horizontal = 4.dp, vertical = 4.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = text,
            color = textColor,
            fontSize = 13.sp,
            lineHeight = 15.sp,
            fontWeight = fontWeight,
            textAlign = TextAlign.Center,
            softWrap = true,
            maxLines = 2
        )
    }
}
