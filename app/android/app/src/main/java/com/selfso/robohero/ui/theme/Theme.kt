package com.selfso.robohero.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

private val DarkColorScheme = darkColorScheme(
    primary = AccentBlue,
    secondary = AccentCyan,
    tertiary = AccentAmber,
    background = BgPrimary,
    surface = BgCard,
    onPrimary = TextMain,
    onSecondary = TextMain,
    onTertiary = BgPrimary,
    onBackground = TextMain,
    onSurface = TextMain
)

@Composable
fun RoboHeroTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        content = content
    )
}
