package com.ximikboda.mremu

import android.Manifest
import android.app.NativeActivity
import android.content.ActivityNotFoundException
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.DocumentsContract
import android.provider.Settings
import android.util.Log
import android.content.Context
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.widget.Toast
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.ViewModelStore
import androidx.lifecycle.ViewModelStoreOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.automirrored.filled.OpenInNew
import androidx.compose.material.icons.filled.CheckBox
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.SdStorage
import androidx.compose.material.icons.outlined.CheckBoxOutlineBlank
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.Typography
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.lifecycle.setViewTreeViewModelStoreOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import androidx.compose.ui.text.ExperimentalTextApi
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontVariation
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import java.io.File
import java.io.FileOutputStream

/** A single Roboto Flex instance with its variable axes pinned. */
@OptIn(ExperimentalTextApi::class)
private fun robotoFlex(weight: Int, opticalSize: Float) = Font(
    resId = R.font.roboto_flex,
    weight = FontWeight(weight),
    variationSettings = FontVariation.Settings(
        FontVariation.Setting("wght", weight.toFloat()),
        // opsz tightens the counters for small text; the expressive screens read at
        // roughly 16sp body and 22sp headers.
        FontVariation.Setting("opsz", opticalSize),
        FontVariation.Setting("wdth", 100f),  // default width, no horizontal stretch
        FontVariation.Setting("GRAD", 0f)
    )
)

/**
 * Roboto Flex, used for the whole sheet. The axes are set per instance above, so the
 * family resolves weights by itself and Material's own weights keep working.
 */
@OptIn(ExperimentalTextApi::class)
private val RobotoFlexFamily = FontFamily(
    robotoFlex(400, 16f),
    robotoFlex(500, 16f),
    robotoFlex(700, 20f)
)

/**
 * Material's default styles retargeted at Roboto Flex. Only the slots the sheet actually
 * uses are listed; anything else keeps the platform default.
 */
@OptIn(ExperimentalTextApi::class)
private val AppTypography = Typography().run {
    copy(
        headlineSmall = headlineSmall.copy(fontFamily = RobotoFlexFamily),
        bodyMedium = bodyMedium.copy(fontFamily = RobotoFlexFamily),
        bodyLarge = bodyLarge.copy(fontFamily = RobotoFlexFamily),
        bodySmall = bodySmall.copy(fontFamily = RobotoFlexFamily),
        titleSmall = titleSmall.copy(fontFamily = RobotoFlexFamily),
        labelLarge = labelLarge.copy(fontFamily = RobotoFlexFamily)
    )
}

class MainActivity : NativeActivity(), LifecycleOwner, ViewModelStoreOwner, SavedStateRegistryOwner {
    private val lifecycleRegistry = LifecycleRegistry(this)
    private val viewModelStoreImpl = ViewModelStore()
    private val savedStateRegistryController = SavedStateRegistryController.create(this)

    override val lifecycle: Lifecycle get() = lifecycleRegistry
    override val viewModelStore: ViewModelStore get() = viewModelStoreImpl
    override val savedStateRegistry: SavedStateRegistry get() = savedStateRegistryController.savedStateRegistry
    companion object {
        private const val TAG = "MREmu"
        private const val STORAGE_PERMISSION_CODE = 100
        private const val MANAGE_STORAGE_REQUEST_CODE = 101
        private const val EXTERNAL_STORAGE_AUTHORITY = "com.android.externalstorage.documents"

        init {
            System.loadLibrary("MREmu")
        }
    }

    private external fun notifyPermissionState(granted: Boolean)
    private external fun nativeLoadVxpFile(path: String)

    /** Asks the native side to perform a hard reset of the running app. */
    private external fun requestHardReset()

    /**
     * Called from the native render thread when the Options button (drawn below the emulated
     * screen) is tapped. Marshals onto the UI thread and presents the native dialog.
     */
    private fun showOptionsDialogUi() {
        runOnUiThread { presentOptionsDialog() }
    }

    /** Builds and shows the Compose bottom sheet. Must run on the UI thread. */
    @OptIn(ExperimentalMaterial3Api::class)
    private fun presentOptionsDialog() {
        val dialog = android.app.Dialog(this, android.R.style.Theme_Translucent_NoTitleBar)
        val composeView = ComposeView(this).apply {
            setViewTreeLifecycleOwner(this@MainActivity)
            setViewTreeViewModelStoreOwner(this@MainActivity)
            setViewTreeSavedStateRegistryOwner(this@MainActivity)
            setContent {
                MaterialTheme(typography = AppTypography) {
                    OptionsBottomSheet(
                        onHardReset = { dialog.dismiss(); performHardReset() },
                        onOpenFolder = { dir -> dialog.dismiss(); openFolder(dir) },
                        onClearUserData = { dialog.dismiss(); performClearUserData() },
                        onAbout = { dialog.dismiss() },
                        onQuit = { dialog.dismiss(); finish() },
                        onDismiss = { dialog.dismiss() }
                    )
                }
            }
        }
        dialog.setContentView(composeView)
        dialog.window?.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT))
        dialog.show()
    }

    /**
     * Opens an emulator folder in a file manager.
     *
     * ACTION_OPEN_DOCUMENT_TREE is the wrong tool here. It is the SAF *picker*: it opens
     * a folder-selection UI that the user has to confirm with "USE THIS FOLDER" instead
     * of simply landing on the path, and EXTRA_INITIAL_URI only asks DocumentsUI to
     * pre-scroll there. That is a different job from "show me this folder".
     *
     * ACTION_VIEW against a folder document URI is what actually navigates a file manager
     * to a location. The URI is an ExternalStorageProvider document URI, whose ids are
     * "<rootId>:<path with no leading slash>", e.g. /sdcard/MREmu/fs/c -> primary:MREmu/fs/c.
     * Uri.fromFile() is not an option: it builds a file:// URI, which StrictMode blocks
     * for outgoing intents since targetSdk 24, so it never reaches any app.
     *
     * Folder MIME types are not standardised, so openFolder() tries the one AOSP defines
     * first and falls back to the legacy values to reach pre-SAF third-party managers.
     *
     * No setPackage() is used on purpose: the DocumentsUI package is
     * com.android.documentsui on AOSP but com.google.android.documentsui on Google
     * builds, and pinning either one throws on the other.
     */
    private fun openFolder(dir: File) {
        if (!dir.exists() && !dir.mkdirs()) {
            Toast.makeText(this, getString(R.string.cannot_create_folder, dir.absolutePath), Toast.LENGTH_SHORT).show()
            return
        }

        // The drive paths are written as /sdcard/..., which is a symlink to
        // /storage/emulated/0 on modern Android. File.absolutePath does not resolve it,
        // so strip either spelling to get the path relative to the emulated root.
        val relativePath = dir.absolutePath
            .removePrefix(Environment.getExternalStorageDirectory().absolutePath)
            .removePrefix("/sdcard")
            .trim('/')
        val docUri = DocumentsContract.buildDocumentUri(EXTERNAL_STORAGE_AUTHORITY, "primary:$relativePath")

        // MIME_TYPE_DIR ("vnd.android.document/directory") is the only folder type the AOSP
        // file manager claims, so it always has a handler on a stock device. "resource/folder"
        // and "inode/directory" are undocumented legacy conventions; only Amaze, Material
        // Files and AppManager answer to them, but they cost nothing to try after the first.
        val mimeTypes = listOf(
            DocumentsContract.Document.MIME_TYPE_DIR,
            "resource/folder",
            "inode/directory"
        )

        for (mime in mimeTypes) {
            // No FLAG_GRANT_READ_URI_PERMISSION: ExternalStorageProvider is the system,
            // not us, and we are handing out a URI we do not own.
            val intent = Intent(Intent.ACTION_VIEW).apply { setDataAndType(docUri, mime) }
            try {
                // Deliberately no resolveActivity() guard. Package visibility filtering on
                // Android 11+ makes resolveActivity() return null for activities that
                // startActivity() would launch fine, so trusting it shows a bogus
                // "no app can open that folder" while a working file manager sits there.
                // Catching the real ActivityNotFoundException cannot lie to the user.
                startActivity(intent)
                return
            } catch (e: ActivityNotFoundException) {
                Log.d(TAG, "No file manager handles folders as $mime")
            }
        }

        // Nothing claimed the folder, so fall back to the SAF picker, pre-positioned.
        // EXTRA_INITIAL_URI needs a root or *document* URI: DocumentsUI silently drops a
        // tree URI and shows the last-used location instead.
        try {
            startActivity(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
                putExtra(DocumentsContract.EXTRA_INITIAL_URI, docUri)
            })
        } catch (e: Exception) {
            // Only reachable on a device with DocumentsUI removed or disabled.
            Log.e(TAG, "Cannot open folder ${dir.absolutePath}", e)
            Toast.makeText(this, getString(R.string.no_app_to_open_folder), Toast.LENGTH_SHORT).show()
        }
    }

    /** Opens a web URL in the default browser, if any app can handle it. */
    private fun openUrl(url: String) {
        try {
            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
        } catch (e: ActivityNotFoundException) {
            // Reachable on a device with no browser at all.
            Log.w(TAG, "No app can open $url", e)
        }
    }

    /** Confirmation now happens on the Hard reset sheet page, so this only runs the reset. */
    private fun performHardReset() {
        requestHardReset()
        Toast.makeText(this, "Reset emulator state requested", Toast.LENGTH_SHORT).show()
    }

    /** Confirmation now happens on the Clear user data sheet page, so this only clears. */
    private fun performClearUserData() {
        var removed = 0
        for (drive in arrayOf("c", "d")) {
            val root = File("/sdcard/MREmu/fs/$drive")
            root.walkBottomUp().forEach { entry ->
                if (entry != root && entry.isFile && entry.delete()) removed++
            }
        }
        Toast.makeText(this, "Cleared user data ($removed files)", Toast.LENGTH_SHORT).show()
    }

    private fun appVersion(): String = try {
        packageManager.getPackageInfo(packageName, 0).versionName.orEmpty()
    } catch (e: Exception) {
        ""
    }

private enum class OptionsPage(val title: String) {
        Main("Options"),
        Folders("Open FS folder"),
        HardReset("Reset emulator state"),
        ClearUserData("Clear user data"),
        About("About")
    }

    /** Item details for the Open FS folder page. */
    private data class Drive(val label: String, val detail: String, val path: String)

    private val drives = listOf(
        Drive("fs/c", "internal storage", "/sdcard/MREmu/fs/c"),
        Drive("fs/d", "internal storage", "/sdcard/MREmu/fs/d"),
        Drive("fs/e", "external storage", "/sdcard/MREmu/fs/e"),
        Drive("fs/e/mre", "VXP apps", "/sdcard/MREmu/fs/e/mre")
    )

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    private fun OptionsBottomSheet(
        onHardReset: () -> Unit,
        onOpenFolder: (File) -> Unit,
        onClearUserData: () -> Unit,
        onAbout: () -> Unit,
        onQuit: () -> Unit,
        onDismiss: () -> Unit
    ) {
        // skipPartiallyExpanded so the sheet opens at Expanded, whose anchor is
// max(0, screenHeight - contentHeight) -> the sheet is exactly as tall as its
// content (capped at the screen), instead of the default half-screen peek.
val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
        var showSheet by remember { mutableStateOf(true) }
        val stack = rememberPageBackStack(OptionsPage.Main)
        val page = stack.current

        if (showSheet) {
            ModalBottomSheet(
                onDismissRequest = { showSheet = false; onDismiss() },
                sheetState = sheetState,
                containerColor = Color(0xFF1B1B1F),
                contentColor = Color.White,
                // Empty handle: the grab bar is drawn by default and the close icon
                // already sits in the header, so it was a duplicate affordance.
                dragHandle = {}
            ) {
                Column(Modifier.fillMaxWidth()) {
                    // Header: back/close on the left, big centred title, no divider.
                    Box(
                        Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 8.dp, vertical = 6.dp)
                    ) {
                        IconButton(
                            onClick = {
                                if (stack.canGoBack) stack.pop()
                                else { showSheet = false; onDismiss() }
                            },
                            modifier = Modifier.align(Alignment.CenterStart)
                        ) {
                            Icon(
                                imageVector = if (stack.canGoBack)
                                    Icons.AutoMirrored.Filled.ArrowBack else Icons.Filled.Close,
                                contentDescription = if (stack.canGoBack) "Back" else "Close",
                                tint = Color(0xFFE6E1E5)
                            )
                        }

                        Text(
                            text = page.title,
                            style = MaterialTheme.typography.headlineSmall,
                            fontWeight = FontWeight.Bold,
                            color = Color(0xFFE6E1E5),
                            modifier = Modifier.align(Alignment.Center)
                        )
                    }

                    AnimatedContent(
                        targetState = page,
                        transitionSpec = {
                            // Forward = deeper in the stack: the incoming page enters from
                            // the right edge and the outgoing one leaves past the left edge.
                            // Going back is the mirror. Both travel the full width so the two
                            // pages swap completely, and clipping keeps a page from drawing
                            // outside the sheet while it slides.
                            val goingForward = targetState.ordinal > initialState.ordinal
                            // slideIn/OutHorizontally animate an IntOffset, so the tween
                            // has to be typed to match or the spec is rejected.
                            val spec = tween<IntOffset>(280, easing = FastOutSlowInEasing)
                            val enter = if (goingForward)
                                slideInHorizontally(spec) { it }
                            else
                                slideInHorizontally(spec) { -it }
                            val exit = if (goingForward)
                                slideOutHorizontally(spec) { -it }
                            else
                                slideOutHorizontally(spec) { it }
                            (enter togetherWith exit).using(SizeTransform(clip = true))
                        },
                        label = "optionsPage"
                    ) { p ->
                        when (p) {
                            OptionsPage.Main -> {
                                Column(Modifier.fillMaxWidth()) {
                                    SegmentHeader(stringResource(R.string.section_emulator))
                                    SegmentGroup {
                                        OptionRow(Icons.Filled.Refresh, "Reset emulator state") {
                                            stack.push(OptionsPage.HardReset)
                                        }
                                        OptionRow(Icons.Filled.DeleteSweep, "Clear user data", divider = false) {
                                            stack.push(OptionsPage.ClearUserData)
                                        }
                                    }
                                    SegmentHeader(stringResource(R.string.section_files))
                                    SegmentGroup {
                                        OptionRow(
                                            Icons.Filled.FolderOpen,
                                            "Open FS folder...",
                                            Icons.AutoMirrored.Filled.KeyboardArrowRight,
                                            divider = false
                                        ) { stack.push(OptionsPage.Folders) }
                                    }
                                    SegmentHeader(stringResource(R.string.section_app))
                                    SegmentGroup {
                                        OptionRow(Icons.Filled.Info, "About") {
                                            stack.push(OptionsPage.About)
                                        }
                                        OptionRow(
                                            Icons.Filled.PowerSettingsNew,
                                            "Quit",
                                            tint = Color(0xFFE98A8A),
                                            divider = false
                                        ) {
                                            showSheet = false; onQuit()
                                        }
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }

                            OptionsPage.Folders -> {
                                Column(Modifier.fillMaxWidth()) {
                                    // These rows leave the app rather than open another page,
                                    // so they get the external-link icon instead of a chevron.
                                    val storage = drives.filterNot { it.label.endsWith("mre") }
                                    val apps = drives.filter { it.label.endsWith("mre") }
                                    SegmentHeader(stringResource(R.string.section_storage))
                                    SegmentGroup {
                                        storage.forEachIndexed { i, drive ->
                                            OptionRow(
                                                icon = Icons.Filled.SdStorage,
                                                label = drive.label,
                                                detail = drive.detail,
                                                trailing = Icons.AutoMirrored.Filled.OpenInNew,
                                                divider = i < storage.lastIndex
                                            ) {
                                                showSheet = false
                                                onOpenFolder(File(drive.path))
                                            }
                                        }
                                    }
                                    SegmentHeader(stringResource(R.string.section_apps))
                                    SegmentGroup {
                                        apps.forEachIndexed { i, drive ->
                                            OptionRow(
                                                icon = Icons.Filled.Folder,
                                                label = drive.label,
                                                detail = drive.detail,
                                                trailing = Icons.AutoMirrored.Filled.OpenInNew,
                                                divider = i < apps.lastIndex
                                            ) {
                                                showSheet = false
                                                onOpenFolder(File(drive.path))
                                            }
                                        }
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }

                            OptionsPage.HardReset -> {
                                var remaining by remember { mutableStateOf(3) }
                                LaunchedEffect(Unit) {
                                    while (remaining > 0) {
                                        delay(1000L)
                                        remaining--
                                    }
                                }
                                Column(Modifier.fillMaxWidth()) {
                                    Text(
                                        text = stringResource(R.string.hard_reset_message),
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = Color(0xFFC7C5D0),
                                        modifier = Modifier.padding(horizontal = 24.dp, vertical = 12.dp)
                                    )
                                    DestructiveButton("Reset", remaining) {
                                        showSheet = false
                                        onHardReset()
                                    }
                                    Spacer(Modifier.height(24.dp))
                                }
                            }

                            OptionsPage.ClearUserData -> {
                                var remaining by remember { mutableStateOf(3) }
                                LaunchedEffect(Unit) {
                                    while (remaining > 0) {
                                        delay(1000L)
                                        remaining--
                                    }
                                }
                                Column(Modifier.fillMaxWidth()) {
                                    Text(
                                        text = stringResource(R.string.clear_data_message),
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = Color(0xFFC7C5D0),
                                        modifier = Modifier.padding(horizontal = 24.dp, vertical = 12.dp)
                                    )
                                    DestructiveButton("Clear", remaining) {
                                        showSheet = false
                                        onClearUserData()
                                    }
                                    Spacer(Modifier.height(24.dp))
                                }
                            }

                            OptionsPage.About -> {
                                val repoUrl = stringResource(R.string.about_repo_url)
                                Column(Modifier.fillMaxWidth()) {
                                    Text(
                                        text = stringResource(R.string.about_message),
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = Color(0xFFC7C5D0),
                                        modifier = Modifier.padding(horizontal = 24.dp, vertical = 8.dp)
                                    )
                                    // Opens a browser rather than another sheet page,
                                    // so it gets the external-link icon like the folder rows.
                                    SegmentGroup {
                                        OptionRow(
                                            icon = Icons.Filled.Code,
                                            label = stringResource(R.string.about_repo),
                                            detail = repoUrl,
                                            trailing = Icons.AutoMirrored.Filled.OpenInNew
                                        ) {
                                            openUrl("https://$repoUrl")
                                        }
                                        OptionRow(
                                            Icons.Filled.Info,
                                            "Version",
                                            detail = appVersion().ifEmpty { "-" },
                                            divider = false,
                                            onClick = {}
                                        )
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /** A label plus optional second line, with an optional leading and trailing icon. */
    @Composable
    private fun OptionRow(
        icon: androidx.compose.ui.graphics.vector.ImageVector,
        label: String,
        trailing: androidx.compose.ui.graphics.vector.ImageVector? = null,
        detail: String? = null,
        tint: Color = Color(0xFFE6E1E5),
        divider: Boolean = true,
        onClick: () -> Unit
    ) {
        Surface(
            onClick = onClick,
            modifier = Modifier.fillMaxWidth(),
            color = Color.Transparent
        ) {
            Column {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 24.dp, vertical = 14.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(20.dp)
                ) {
                    Icon(
                        imageVector = icon,
                        contentDescription = null,
                        tint = tint,
                        modifier = Modifier.size(22.dp)
                    )
                    Column(Modifier.weight(1f)) {
                        Text(
                            text = label,
                            style = MaterialTheme.typography.bodyLarge,
                            color = tint
                        )
                        if (detail != null) {
                            Text(
                                text = detail,
                                style = MaterialTheme.typography.bodySmall,
                                color = Color(0xFF8E8E93)
                            )
                        }
                    }
                    if (trailing != null) {
                        Icon(
                            imageVector = trailing,
                            contentDescription = null,
                            tint = Color(0xFF8E8E93)
                        )
                    }
                }
                // Drawn inside the row's own surface so the separator stays part of the
                // touch target rather than becoming a gap between two targets.
                if (divider) {
                    HorizontalDivider(
                        color = Color(0xFF34343A),
                        modifier = Modifier.padding(start = 66.dp)
                    )
                }
            }
        }
    }

    /**
     * Section label above a group of rows, matching the Material 3 Expressive pattern.
     */
    @Composable
    private fun SegmentHeader(text: String) {
        Text(
            text = text,
            style = MaterialTheme.typography.titleSmall,
            color = Color(0xFFC7C5D0),
            modifier = Modifier.padding(start = 24.dp, end = 24.dp, top = 16.dp, bottom = 8.dp)
        )
    }

    /**
     * A run of rows that touch each other inside one rounded card. Expressive draws the
     * separator as a hairline inset to the text start rather than a full-bleed rule, so
     * the group reads as a single object instead of a stack of separate list items.
     */
    @Composable
    private fun SegmentGroup(
        modifier: Modifier = Modifier,
        content: @Composable ColumnScope.() -> Unit
    ) {
        Column(
            modifier = modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp)
                .clip(RoundedCornerShape(24.dp))
                .background(Color(0xFF232327))
                .border(1.dp, Color(0xFF34343A), RoundedCornerShape(24.dp))
        ) {
            content()
        }
    }

    /**
     * A destructive confirm button that, instead of needing a separate checkbox, arms
     * itself with a 3..1 countdown on the first tap. During the countdown the label is
     * replaced by just the number; the action fires when the countdown reaches zero.
     */
    @Composable
    private fun DestructiveButton(label: String, remaining: Int, onConfirmed: () -> Unit) {
        val red = Color(0xFFC62828)
        Button(
            onClick = onConfirmed,
            enabled = remaining == 0,
            colors = ButtonDefaults.buttonColors(
                containerColor = red,
                contentColor = Color.White,
                disabledContainerColor = red.copy(alpha = 0.30f),
                disabledContentColor = Color.White.copy(alpha = 0.45f)
            ),
            modifier = Modifier
                .fillMaxWidth()
                .height(72.dp)
                .padding(horizontal = 32.dp, vertical = 16.dp)
        ) {
            Text(
                text = if (remaining > 0) "${remaining}s" else label,
                style = MaterialTheme.typography.bodyLarge
            )
        }
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        savedStateRegistryController.performRestore(savedInstanceState)
        super.onCreate(savedInstanceState)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_CREATE)

        checkAndRequestStoragePermission()

        hideSystemUI()

        handleIntent(intent)
    }

    override fun onResume() {
        super.onResume()
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
    }

    override fun onPause() {
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_PAUSE)
        super.onPause()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        savedStateRegistryController.performSave(outState)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUI()
        }
    }

    private fun checkAndRequestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                Log.d("MREmu", "Requesting all files access (Android 11+)")
                try {
                    val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
                    intent.addCategory("android.intent.category.DEFAULT")
                    intent.data = Uri.parse(String.format("package:%s", packageName))
                    startActivityForResult(intent, MANAGE_STORAGE_REQUEST_CODE)
                } catch (e: Exception) {
                    val intent = Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                    startActivityForResult(intent, MANAGE_STORAGE_REQUEST_CODE)
                }
            } else {
                Log.d("MREmu", "All files access already granted")
                notifyPermissionState(true)
            }
        } else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE), STORAGE_PERMISSION_CODE)
                } else {
                    notifyPermissionState(true)
                }
            } else {
                notifyPermissionState(true)
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == MANAGE_STORAGE_REQUEST_CODE) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                if (Environment.isExternalStorageManager()) {
                    Log.d("MREmu", "User granted MANAGE_EXTERNAL_STORAGE")
                    notifyPermissionState(true)
                } else {
                    Log.e("MREmu", "User denied MANAGE_EXTERNAL_STORAGE")
                    notifyPermissionState(false)
                }
            }
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == STORAGE_PERMISSION_CODE) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.d("MREmu", "User granted WRITE_EXTERNAL_STORAGE")
                notifyPermissionState(true)
            } else {
                Log.e("MREmu", "User denied WRITE_EXTERNAL_STORAGE")
                notifyPermissionState(false)
            }
        }
    }

    private fun removeAppBar() {
        // No-op: the Compose bottom sheet is dismissed via its own state.
    }

    private fun hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.let { controller ->
                controller.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                controller.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            or View.SYSTEM_UI_FLAG_FULLSCREEN
                    )
        }
    }

    private fun handleIntent(intent: Intent?) {
        if (intent?.action == Intent.ACTION_VIEW) {
            val uri = intent.data
            if (uri != null) {
                Thread {
                    try {
                        val tempVxpFile = File(cacheDir, "opened_app.vxp")

                        val inputStream = contentResolver.openInputStream(uri)
                        val outputStream = FileOutputStream(tempVxpFile)

                        inputStream?.use { input ->
                            outputStream.use { output ->
                                input.copyTo(output)
                            }
                        }

                        val realPosixPath = tempVxpFile.absolutePath
                        Log.d("MREmu", "File copied to cache: $realPosixPath")

                        runOnUiThread {
                            nativeLoadVxpFile(realPosixPath)
                        }

                    } catch (e: Exception) {
                        Log.e("MREmu", "Coping to cache error", e)
                    }
                }.start()
            }
        }
    }

    fun triggerVibration(durationMs: Long) {
        val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vibratorManager = getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
            vibratorManager.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createOneShot(durationMs, VibrationEffect.DEFAULT_AMPLITUDE))
        } else {
            @Suppress("DEPRECATION")
            vibrator.vibrate(durationMs)
        }
    }

    fun hasVibrator(): Boolean {
        val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vibratorManager = getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
            vibratorManager.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
        }

        return vibrator.hasVibrator()
    }
}
