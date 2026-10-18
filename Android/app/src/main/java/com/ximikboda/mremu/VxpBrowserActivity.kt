package com.ximikboda.mremu

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Language
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

/**
 * Placeholder screen for the VXP Browser, reached from the VXP library header menu.
 *
 * There is no browser yet: this exists so the menu entry is wired to a real destination
 * rather than a no-op, which keeps the navigation path visible while the browser itself
 * is still to be built. It deliberately shares the sheet's dark palette so it does not
 * flash white on the way in.
 */
class VxpBrowserActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(typography = AppTypography) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = Color(0xFF1B1B1F)
                ) {
                    Column {
                        // targetSdk 36 enforces edge-to-edge, so without this the header
                        // row draws underneath the status bar and its back button and
                        // title end up half hidden behind the clock and battery icons.
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .statusBarsPadding()
                                .padding(horizontal = 8.dp, vertical = 6.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            IconButton(onClick = { finish() }) {
                                Icon(
                                    imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                                    contentDescription = "Back",
                                    tint = Color(0xFFE6E1E5)
                                )
                            }
                            Text(
                                text = stringResource(R.string.vxp_browser),
                                style = MaterialTheme.typography.headlineSmall,
                                fontWeight = FontWeight.Bold,
                                color = Color(0xFFE6E1E5),
                                modifier = Modifier.padding(start = 4.dp)
                            )
                        }

                        Box(
                            modifier = Modifier.fillMaxSize(),
                            contentAlignment = Alignment.Center
                        ) {
                            Column(
                                horizontalAlignment = Alignment.CenterHorizontally,
                                verticalArrangement = Arrangement.spacedBy(12.dp)
                            ) {
                                Box(
                                    modifier = Modifier
                                        .size(64.dp)
                                        .clip(RoundedCornerShape(20.dp))
                                        .background(Color(0xFF2F2F36)),
                                    contentAlignment = Alignment.Center
                                ) {
                                    Icon(
                                        imageVector = Icons.Filled.Language,
                                        contentDescription = null,
                                        tint = Color(0xFFC7C5D0),
                                        modifier = Modifier.size(30.dp)
                                    )
                                }
                                Text(
                                    text = stringResource(R.string.vxp_browser_placeholder),
                                    style = MaterialTheme.typography.titleSmall,
                                    color = Color(0xFFC7C5D0)
                                )
                                Text(
                                    text = stringResource(R.string.vxp_browser_placeholder_desc),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = Color(0xFF8E8E93)
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}