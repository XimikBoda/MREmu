package com.ximikboda.mremu

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material3.MenuDefaults
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.OpenInNew
import androidx.compose.material.icons.filled.Clear
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.DataUsage
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Event
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.Sort
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.TextFields
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.compose.animation.animateContentSize
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import java.io.File

enum class VxpSort(val label: String) {
    Name("Name"),
    Date("Date"),
    Size("Size")
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun VxpLibraryPage(
    onLaunch: (VxpItem) -> Unit,
    onShare: (VxpItem) -> Unit,
    onRevealFolder: (File) -> Unit
) {
    val context = LocalContext.current
    val repository = remember { VxpRepository(context) }
    // Point the icon loader at the on-disk cache once, so icons survive process death
    // instead of being decoded again on every launch.
    LaunchedEffect(Unit) { VxpIconLoader.init(context.applicationContext) }
    var items by remember { mutableStateOf<List<VxpItem>>(emptyList()) }
    var searchQuery by remember { mutableStateOf("") }
    var selectedSort by remember { mutableStateOf(VxpSort.Name) }
    var sortAscending by remember { mutableStateOf(true) }
    var onlyRecents by remember { mutableStateOf(false) }

    // Dialog states
    var itemToDelete by remember { mutableStateOf<VxpItem?>(null) }
    var itemToRename by remember { mutableStateOf<VxpItem?>(null) }
    var renameText by remember { mutableStateOf("") }

    fun refreshList() {
        items = repository.loadAllVxps()
    }

    LaunchedEffect(Unit) {
        refreshList()
    }

    val filteredItems = remember(items, searchQuery, selectedSort, sortAscending, onlyRecents) {
        var result = items.asSequence()
        if (onlyRecents) {
            result = result.filter { it.isRecent }
        }
        if (searchQuery.isNotBlank()) {
            val q = searchQuery.trim()
            result = result.filter { it.name.contains(q, ignoreCase = true) }
        }
        val comparator = when (selectedSort) {
            VxpSort.Name -> compareBy<VxpItem> { it.name.lowercase() }
            VxpSort.Date -> compareBy<VxpItem> { it.lastModified }
            VxpSort.Size -> compareBy<VxpItem> { it.sizeBytes }
        }
        val sorted = if (sortAscending) result.sortedWith(comparator) else result.sortedWith(comparator.reversed())
        sorted.toList()
    }

    Column(
        Modifier
            .fillMaxWidth()
            .animateContentSize()
            .padding(bottom = 16.dp)
    ) {
        // Material 3 search field. Built by hand rather than with the `SearchBar`
        // composable: that widget reserves a wrapper surface plus vertical padding
        // around its input slot, which showed up as dead space under the page header.
        Surface(
            shape = RoundedCornerShape(28.dp),
            color = Color(0xFF232327),
            // tonalElevation tints the surface with the theme primary, which put a
            // blue cast on the pill; zero keeps it neutral like the rest of the page.
            tonalElevation = 0.dp,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 4.dp)
        ) {
            TextField(
                value = searchQuery,
                onValueChange = { searchQuery = it },
                modifier = Modifier.fillMaxWidth(),
                placeholder = {
                    Text(
                        text = stringResource(R.string.vxp_search_hint),
                        style = MaterialTheme.typography.bodyLarge,
                        color = Color(0xFF8E8E93)
                    )
                },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Filled.Search,
                        contentDescription = null,
                        tint = Color(0xFFC7C5D0),
                        modifier = Modifier.size(22.dp)
                    )
                },
                trailingIcon = {
                    if (searchQuery.isNotEmpty()) {
                        IconButton(onClick = { searchQuery = "" }) {
                            Icon(
                                imageVector = Icons.Filled.Clear,
                                contentDescription = "Clear search",
                                tint = Color(0xFFC7C5D0),
                                modifier = Modifier.size(20.dp)
                            )
                        }
                    }
                },
                singleLine = true,
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = Color.Transparent,
                    unfocusedContainerColor = Color.Transparent,
                    disabledContainerColor = Color.Transparent,
                    focusedIndicatorColor = Color.Transparent,
                    unfocusedIndicatorColor = Color.Transparent,
                    disabledIndicatorColor = Color.Transparent,
                    // Cursor matches the neutral palette instead of the theme blue.
                    cursorColor = Color(0xFFC7C5D0),
                    focusedTextColor = Color.White,
                    unfocusedTextColor = Color(0xFFE6E1E5)
                )
            )
        }

        // Filter chips and Sort toggle
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            FilterChip(
                selected = !onlyRecents,
                onClick = { onlyRecents = false },
                label = { Text(stringResource(R.string.vxp_filter_all), style = MaterialTheme.typography.bodySmall) },
                colors = FilterChipDefaults.filterChipColors(
                    selectedContainerColor = Color(0xFF3B3B42),
                    selectedLabelColor = Color.White,
                    containerColor = Color(0xFF1F1F23),
                    labelColor = Color(0xFF8E8E93)
                ),
                border = FilterChipDefaults.filterChipBorder(
                    borderColor = Color(0xFF34343A),
                    selectedBorderColor = Color(0xFF5A5A65),
                    enabled = true,
                    selected = !onlyRecents
                )
            )
            FilterChip(
                selected = onlyRecents,
                onClick = { onlyRecents = true },
                label = { Text(stringResource(R.string.vxp_filter_recents), style = MaterialTheme.typography.bodySmall) },
                colors = FilterChipDefaults.filterChipColors(
                    selectedContainerColor = Color(0xFF3B3B42),
                    selectedLabelColor = Color.White,
                    containerColor = Color(0xFF1F1F23),
                    labelColor = Color(0xFF8E8E93)
                ),
                border = FilterChipDefaults.filterChipBorder(
                    borderColor = Color(0xFF34343A),
                    selectedBorderColor = Color(0xFF5A5A65),
                    enabled = true,
                    selected = onlyRecents
                )
            )

            Spacer(Modifier.weight(1f))

            // Sort selector dropdown
            var sortMenuOpen by remember { mutableStateOf(false) }
            Box {
                Row(
                    modifier = Modifier
                        .clip(RoundedCornerShape(12.dp))
                        .background(Color(0xFF232327))
                        .border(1.dp, Color(0xFF34343A), RoundedCornerShape(12.dp))
                        .clickable { sortMenuOpen = true }
                        .padding(horizontal = 10.dp, vertical = 6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    Icon(
                        imageVector = Icons.Filled.Sort,
                        contentDescription = null,
                        tint = Color(0xFF8E8E93),
                        modifier = Modifier.size(16.dp)
                    )
                    Text(
                        text = selectedSort.label,
                        style = MaterialTheme.typography.bodySmall,
                        color = Color(0xFFE6E1E5)
                    )
                    Icon(
                        imageVector = if (sortAscending)
                            Icons.Filled.KeyboardArrowUp else Icons.Filled.KeyboardArrowDown,
                        contentDescription = null,
                        tint = Color(0xFF8E8E93),
                        modifier = Modifier.size(16.dp)
                    )
                }

                VxpContextMenu(
                    expanded = sortMenuOpen,
                    onDismissRequest = { sortMenuOpen = false }
                ) {
                    VxpSort.values().forEach { sort ->
                        VxpContextMenuItem(
                            label = sort.label,
                            icon = when (sort) {
                                VxpSort.Name -> Icons.Filled.TextFields
                                VxpSort.Date -> Icons.Filled.Event
                                VxpSort.Size -> Icons.Filled.DataUsage
                            },
                            highlighted = selectedSort == sort,
                            // Arrow glyph instead of an "(Asc)"/"(Desc)" suffix, so the
                            // direction is read at a glance rather than parsed.
                            trailingIcon = if (selectedSort == sort) {
                                {
                                    Icon(
                                        imageVector = if (sortAscending)
                                            Icons.Filled.KeyboardArrowUp else Icons.Filled.KeyboardArrowDown,
                                        contentDescription = null,
                                        tint = Color(0xFFC7C5D0),
                                        modifier = Modifier.size(20.dp)
                                    )
                                }
                            } else null,
                            onClick = {
                                if (selectedSort == sort) {
                                    sortAscending = !sortAscending
                                } else {
                                    selectedSort = sort
                                    sortAscending = true
                                }
                                sortMenuOpen = false
                            }
                        )
                    }
                }
            }
        }

        Spacer(Modifier.height(8.dp))

        // VXP File List
        if (filteredItems.isEmpty()) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(32.dp),
                contentAlignment = Alignment.Center
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(
                        imageVector = Icons.Filled.SportsEsports,
                        contentDescription = null,
                        tint = Color(0xFF5A5A65),
                        modifier = Modifier.size(48.dp)
                    )
                    Spacer(Modifier.height(12.dp))
                    Text(
                        text = stringResource(R.string.vxp_empty_title),
                        style = MaterialTheme.typography.titleSmall,
                        color = Color(0xFFC7C5D0),
                        textAlign = TextAlign.Center,
                        modifier = Modifier.fillMaxWidth()
                    )
                    Spacer(Modifier.height(4.dp))
                    Text(
                        text = stringResource(R.string.vxp_empty_desc),
                        style = MaterialTheme.typography.bodySmall,
                        color = Color(0xFF8E8E93),
                        textAlign = TextAlign.Center,
                        modifier = Modifier
                            .fillMaxWidth()
                            .widthIn(max = 280.dp)
                            .padding(horizontal = 24.dp)
                    )
                }
            }
        } else {
            Box(Modifier.animateContentSize()) {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 380.dp)
                        .padding(horizontal = 16.dp)
                        .clip(RoundedCornerShape(24.dp))
                        .background(Color(0xFF232327))
                        .border(1.dp, Color(0xFF34343A), RoundedCornerShape(24.dp))
                ) {
                    items(filteredItems, key = { it.path }) { item ->
                        val index = filteredItems.indexOf(item)
                        VxpItemRow(
                            item = item,
                            divider = index < filteredItems.lastIndex,
                            onLaunch = { onLaunch(item) },
                            onRename = {
                                itemToRename = item
                                renameText = item.name.removeSuffix(".vxp")
                            },
                            onShare = { onShare(item) },
                            onDelete = { itemToDelete = item },
                            onReveal = {
                                item.file.parentFile?.let { onRevealFolder(it) }
                            }
                        )
                    }
                }
            }
        }
    }

    // Delete Confirmation Dialog
    itemToDelete?.let { target ->
        AlertDialog(
            onDismissRequest = { itemToDelete = null },
            title = {
                Text(stringResource(R.string.vxp_delete_confirm_title), color = Color.White)
            },
            text = {
                Text(
                    stringResource(R.string.vxp_delete_confirm_desc, target.name),
                    color = Color(0xFFC7C5D0)
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        repository.deleteVxp(target)
                        itemToDelete = null
                        refreshList()
                    }
                ) {
                    Text(stringResource(R.string.delete), color = Color(0xFFE98A8A))
                }
            },
            dismissButton = {
                TextButton(onClick = { itemToDelete = null }) {
                    Text(stringResource(R.string.cancel), color = Color.White)
                }
            },
            containerColor = Color(0xFF232327)
        )
    }

    // Rename Dialog
    itemToRename?.let { target ->
        AlertDialog(
            onDismissRequest = { itemToRename = null },
            title = {
                Text(stringResource(R.string.vxp_rename_title), color = Color.White)
            },
            text = {
                Column {
                    OutlinedTextField(
                        value = renameText,
                        onValueChange = { renameText = it },
                        label = {
                            Text(
                                stringResource(R.string.vxp_rename_hint),
                                color = Color(0xFF8E8E93)
                            )
                        },
                        singleLine = true,
                        // Neutral outlines: the default theme primary paints the focused
                        // border and label blue, which clashed with the dark sheet.
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedTextColor = Color.White,
                            unfocusedTextColor = Color(0xFFE6E1E5),
                            focusedBorderColor = Color(0xFF5A5A65),
                            unfocusedBorderColor = Color(0xFF34343A),
                            focusedLabelColor = Color(0xFFC7C5D0),
                            unfocusedLabelColor = Color(0xFF8E8E93),
                            cursorColor = Color(0xFFC7C5D0)
                        ),
                        shape = RoundedCornerShape(14.dp)
                    )
                }
            },
            // Stacked full-width actions inside one segmented card, matching the
            // list-view rows, instead of Material's side-by-side text buttons.
            confirmButton = {
                VxpDialogActions(
                    primaryLabel = stringResource(R.string.rename),
                    primaryEnabled = renameText.isNotBlank(),
                    onPrimary = {
                        val result = repository.renameVxp(target, renameText.trim())
                        itemToRename = null
                        if (result != null) {
                            refreshList()
                        }
                    },
                    onDismiss = { itemToRename = null }
                )
            },
            containerColor = Color(0xFF232327)
        )
    }
}

/**
 * ChatGPT-style popup menu: a raised dark card with generous 24dp icon gutters, tall
 * rows and no dividers. Material's default [DropdownMenu] packs the icon flush against
 * the text, which reads as a dense Android dialog instead of an iOS-style action sheet.
 */
@Composable
private fun VxpContextMenu(
    expanded: Boolean,
    onDismissRequest: () -> Unit,
    content: @Composable ColumnScope.() -> Unit
) {
    DropdownMenu(
        expanded = expanded,
        onDismissRequest = onDismissRequest,
        containerColor = Color(0xFF2B2B30),
        shape = RoundedCornerShape(16.dp),
        shadowElevation = 12.dp,
        tonalElevation = 0.dp,
        modifier = Modifier
            .padding(horizontal = 8.dp)
            .widthIn(min = 180.dp)
    ) {
        Column(
            modifier = Modifier.padding(4.dp),
            content = content
        )
    }
}

/**
 * Dialog action area: a primary and a cancel row stacked in one rounded segmented card,
 * the same treatment the options list uses. Material's `confirmButton`/`dismissButton`
 * slots lay them out side by side as plain text buttons, which reads as a different
 * design language from the rest of the sheet.
 */
@Composable
private fun VxpDialogActions(
    primaryLabel: String,
    primaryEnabled: Boolean,
    onPrimary: () -> Unit,
    onDismiss: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 16.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(Color(0xFF2F2F36))
            .border(1.dp, Color(0xFF34343A), RoundedCornerShape(16.dp))
    ) {
        VxpDialogActionRow(label = primaryLabel, primary = true, enabled = primaryEnabled, onClick = onPrimary)
        HorizontalDivider(color = Color(0xFF34343A))
        VxpDialogActionRow(label = stringResource(R.string.cancel), onClick = onDismiss)
    }
}

/** One full-width row of [VxpDialogActions]; `primary` gets the brighter text. */
@Composable
private fun VxpDialogActionRow(
    label: String,
    onClick: () -> Unit,
    primary: Boolean = false,
    enabled: Boolean = true
) {
    Surface(
        onClick = onClick,
        enabled = enabled,
        modifier = Modifier.fillMaxWidth(),
        color = Color.Transparent
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 16.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
                fontWeight = if (primary) FontWeight.SemiBold else FontWeight.Normal,
                color = when {
                    !enabled -> Color(0xFF5A5A65)
                    primary -> Color.White
                    else -> Color(0xFFC7C5D0)
                }
            )
        }
    }
}

/** One row of [VxpContextMenu]; `destructive` paints the row in the app's delete red. */
@Composable
private fun VxpContextMenuItem(
    label: String,
    icon: ImageVector,
    onClick: () -> Unit,
    destructive: Boolean = false,
    highlighted: Boolean = false,
    trailingIcon: (@Composable () -> Unit)? = null
) {
    val tint = when {
        destructive -> Color(0xFFE98A8A)
        highlighted -> Color.White
        else -> Color(0xFFE6E1E5)
    }
    DropdownMenuItem(
        text = {
            Text(
                text = label,
                style = MaterialTheme.typography.titleMedium,
                color = tint
            )
        },
        leadingIcon = {
            Icon(
                imageVector = icon,
                contentDescription = null,
                tint = tint,
                modifier = Modifier.size(24.dp)
            )
        },
        trailingIcon = trailingIcon,
        onClick = onClick,
        colors = MenuDefaults.itemColors(
            textColor = tint,
            leadingIconColor = tint
        ),
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 8.dp)
    )
}

/** Icon states for a library row. Kept explicit so "not yet known" is not confused with "none". */
private sealed interface LoadState {
    data object Loading : LoadState
    data class Ready(val bitmap: ImageBitmap) : LoadState
    data object Missing : LoadState
}

@Composable
private fun VxpItemRow(
    item: VxpItem,
    divider: Boolean,
    onLaunch: () -> Unit,
    onRename: () -> Unit,
    onShare: () -> Unit,
    onDelete: () -> Unit,
    onReveal: () -> Unit
) {
    var menuOpen by remember { mutableStateOf(false) }

    // Decoding starts off the main thread: this parses the VXP container and decodes an
    // image, neither of which belongs in a scroll frame. Sealed (null) until resolved,
    // and null for good when the package has no decodable icon.
    val loadState by produceState<LoadState>(LoadState.Loading, item.path, item.lastModified) {
        value = withContext(Dispatchers.IO) {
            val bitmap = VxpIconLoader.load(item.file)
            if (bitmap != null) LoadState.Ready(bitmap) else LoadState.Missing
        }
    }

    Surface(
        onClick = onLaunch,
        modifier = Modifier.fillMaxWidth(),
        color = Color.Transparent
    ) {
        Column {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // The icon slot is a fixed-size box whichever state it is in. Sizing it to the
                // content instead would make every row's text shift sideways as each
                // icon resolved, which is very visible while scrolling.
                Box(
                    modifier = Modifier.size(40.dp),
                    contentAlignment = Alignment.Center
                ) {
                    when (val state = loadState) {
                        is LoadState.Ready -> Image(
                            bitmap = state.bitmap,
                            contentDescription = null,
                            // Package icons are 45x45 but drawn at 40dp, so scale down
                            // rather than stretching the bitmap to fill the box.
                            modifier = Modifier.size(40.dp)
                        )
                        LoadState.Loading -> CircularProgressIndicator(
                            modifier = Modifier.size(20.dp),
                            color = Color(0xFF8E8E93),
                            strokeWidth = 2.dp
                        )
                        LoadState.Missing -> Icon(
                            imageVector = Icons.Filled.SportsEsports,
                            contentDescription = null,
                            tint = Color(0xFF5A5A65),
                            modifier = Modifier.size(26.dp)
                        )
                    }
                }

                // Name and details
                Column(Modifier.weight(1f)) {
                    Text(
                        text = item.name,
                        style = MaterialTheme.typography.bodyLarge,
                        fontWeight = FontWeight.Medium,
                        color = Color(0xFFE6E1E5),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Spacer(Modifier.height(2.dp))
                    Text(
                        text = "${item.formattedSize} • ${item.formattedDate}",
                        style = MaterialTheme.typography.bodySmall,
                        color = Color(0xFF8E8E93),
                        maxLines = 1
                    )
                }

                // Quick Launch button
                IconButton(
                    onClick = onLaunch,
                    modifier = Modifier.size(32.dp)
                ) {
                    Icon(
                        imageVector = Icons.Filled.PlayArrow,
                        contentDescription = stringResource(R.string.vxp_action_launch),
                        tint = Color(0xFF8AB4F8),
                        modifier = Modifier.size(22.dp)
                    )
                }

                // Overflow menu button
                Box {
                    IconButton(
                        onClick = { menuOpen = true },
                        modifier = Modifier.size(32.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Filled.MoreVert,
                            contentDescription = "Options",
                            tint = Color(0xFF8E8E93),
                            modifier = Modifier.size(20.dp)
                        )
                    }

                    VxpContextMenu(
                        expanded = menuOpen,
                        onDismissRequest = { menuOpen = false }
                    ) {
                        VxpContextMenuItem(
                            label = stringResource(R.string.vxp_action_launch),
                            icon = Icons.Filled.PlayArrow,
                            onClick = { menuOpen = false; onLaunch() }
                        )
                        VxpContextMenuItem(
                            label = stringResource(R.string.vxp_action_rename),
                            icon = Icons.Filled.Edit,
                            onClick = { menuOpen = false; onRename() }
                        )
                        VxpContextMenuItem(
                            label = stringResource(R.string.vxp_action_share),
                            icon = Icons.Filled.Share,
                            onClick = { menuOpen = false; onShare() }
                        )
                        VxpContextMenuItem(
                            label = stringResource(R.string.vxp_action_reveal),
                            icon = Icons.Filled.FolderOpen,
                            onClick = { menuOpen = false; onReveal() }
                        )
                        VxpContextMenuItem(
                            label = stringResource(R.string.vxp_action_delete),
                            icon = Icons.Filled.Delete,
                            destructive = true,
                            onClick = { menuOpen = false; onDelete() }
                        )
                    }
                }
            }

            if (divider) {
                HorizontalDivider(
                    color = Color(0xFF34343A),
                    // Aligns with the text column: 16dp row padding + 26dp icon + 16dp gap.
                    modifier = Modifier.padding(start = 58.dp)
                )
            }
        }
    }
}
