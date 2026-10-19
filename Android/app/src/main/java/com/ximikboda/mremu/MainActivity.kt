package com.ximikboda.mremu

import android.Manifest
import android.app.Activity
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
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.automirrored.filled.OpenInNew
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.CheckBox
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.ErrorOutline
import androidx.compose.material.icons.filled.FileOpen
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.SdStorage
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.outlined.CheckBoxOutlineBlank
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
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
import androidx.compose.runtime.rememberCoroutineScope
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
 * uses are listed; anything else keeps the platform default. Internal rather than private
 * so other screens in this package can share the same type ramp.
 */
@OptIn(ExperimentalTextApi::class)
internal val AppTypography = Typography().run {
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

    /**
     * The options sheet, held so that onDestroy can dismiss it and so a second
     * Options tap cannot stack two sheets on the same activity.
     */
    private var optionsDialog: android.app.Dialog? = null

    /**
     * The fatal-error dialog. Held separately from the options sheet so a second error
     * cannot stack two dialogs, and dismissed in onDestroy with the rest of the windows.
     */
    private var errorDialog: android.app.Dialog? = null

    override val lifecycle: Lifecycle get() = lifecycleRegistry
    override val viewModelStore: ViewModelStore get() = viewModelStoreImpl
    override val savedStateRegistry: SavedStateRegistry get() = savedStateRegistryController.savedStateRegistry
    companion object {
        private const val TAG = "MREmu"
        private const val STORAGE_PERMISSION_CODE = 100
        private const val MANAGE_STORAGE_REQUEST_CODE = 101
        private const val EXTERNAL_STORAGE_AUTHORITY = "com.android.externalstorage.documents"
        private const val PICK_VXP_REQUEST_CODE = 102

        // Vibration shaping, in milliseconds. See triggerVibration for why the same
        // duration needs different treatment depending on the device's motor.
        // TICK_MAX_MS and PREDEFINED_MAX_MS split the OEM-tuned effect band: at or below
        // TICK_MAX_MS the pulse is a discrete tick (what the app selector fires per
        // selection change), above it and up to PREDEFINED_MAX_MS it is a firmer click,
        // and above that it is a sustained buzz that no predefined effect models.
        private const val TICK_MAX_MS = 100L
        private const val PREDEFINED_MAX_MS = 250L
        // Shortest pulse an eccentric rotating mass motor will actually produce, used
        // only on API < 29 where there are no predefined effects to defer to the OEM.
        private const val ROTATING_MASS_FLOOR_MS = 60L

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
        // This block is posted from the native render thread, and the native side
        // caches a single global ref to the activity that is never refreshed, so by
        // the time it runs the activity may already be finishing. Dialog.show()
        // against a dead window token throws BadTokenException on the main thread,
        // which takes the whole process down.
        if (isFinishing || isDestroyed) return
        if (optionsDialog?.isShowing == true) return

        val stack = PageBackStack(listOf(OptionsPage.Main))
        val dialog = object : android.app.Dialog(this, android.R.style.Theme_Translucent_NoTitleBar) {
            @Deprecated("Deprecated in Java")
            override fun onBackPressed() {
                if (stack.canGoBack) {
                    stack.pop()
                } else {
                    super.onBackPressed()
                }
            }
        }
        optionsDialog = dialog
        dialog.setOnDismissListener {
            if (optionsDialog === dialog) optionsDialog = null
        }
        val composeView = ComposeView(this).apply {
            setViewTreeLifecycleOwner(this@MainActivity)
            setViewTreeViewModelStoreOwner(this@MainActivity)
            setViewTreeSavedStateRegistryOwner(this@MainActivity)
            setContent {
                MaterialTheme(typography = AppTypography) {
                    OptionsBottomSheet(
                        stack = stack,
                        onLaunchVxp = { file -> dialog.dismiss(); launchVxp(file) },
                        onShareVxp = { file -> shareVxp(file) },
                        onHardReset = { dialog.dismiss(); performHardReset() },
                        onOpenFolder = { dir -> dialog.dismiss(); openFolder(dir) },
                        onClearUserData = { dialog.dismiss(); performClearUserData() },
                        onAbout = { dialog.dismiss() },
                        onQuit = { dialog.dismiss(); finish() },
                        onOpenVxpBrowser = { dialog.dismiss(); openVxpBrowser() },
                        onImportVxp = { dialog.dismiss(); openVxpPicker() },
                        onDismiss = { dialog.dismiss() }
                    )
                }
            }
        }
        dialog.setContentView(composeView)
        dialog.window?.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT))
        try {
            dialog.show()
        } catch (e: android.view.WindowManager.BadTokenException) {
            Log.w(TAG, "Options dialog could not be shown", e)
            if (optionsDialog === dialog) optionsDialog = null
        }
    }

    /**
     * Called from the native render thread when the emulator raises a fatal error
     * (unrecognised VXP, failed load, preparation fault). Marshals onto the UI thread
     * and presents a dialog with the message and a copy button.
     */
    private fun showErrorDialogUi(message: String) {
        runOnUiThread { presentErrorDialog(message) }
    }

    /** Builds and shows the Compose error dialog. Must run on the UI thread. */
    private fun presentErrorDialog(message: String) {
        // Posted from the native render thread against a cached global ref that is never
        // refreshed, so the activity may already be gone by the time this runs.
        if (isFinishing || isDestroyed) return
        if (errorDialog?.isShowing == true) return

        val dialog = android.app.Dialog(this, android.R.style.Theme_Translucent_NoTitleBar)
        errorDialog = dialog
        dialog.setOnDismissListener {
            if (errorDialog === dialog) errorDialog = null
        }

        val composeView = ComposeView(this).apply {
            setViewTreeLifecycleOwner(this@MainActivity)
            setViewTreeViewModelStoreOwner(this@MainActivity)
            setViewTreeSavedStateRegistryOwner(this@MainActivity)
            setContent {
                MaterialTheme(typography = AppTypography) {
                    // The translucent platform Dialog above already provides the overlay
                    // and window, so the card is laid out directly rather than wrapping
                    // it in a Compose Dialog, which would open a second window.
                    ErrorDialog(
                        message = message,
                        onCopy = { copyToClipboard(message) },
                        onDismiss = { dialog.dismiss() }
                    )
                }
            }
        }
        dialog.setContentView(composeView)
        dialog.window?.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT))
        dialog.window?.setLayout(
            android.view.WindowManager.LayoutParams.MATCH_PARENT,
            android.view.WindowManager.LayoutParams.WRAP_CONTENT
        )
        try {
            dialog.show()
        } catch (e: android.view.WindowManager.BadTokenException) {
            Log.w(TAG, "Error dialog could not be shown", e)
            if (errorDialog === dialog) errorDialog = null
        }
    }

    /** Opens the placeholder VXP browser screen. */
    private fun openVxpBrowser() {
        startActivity(Intent(this, VxpBrowserActivity::class.java))
    }

    /**
 * Copies [text] to the clipboard. Returns false if the clipboard was unavailable.
 *
 * The caller is responsible for confirming in the UI. A system toast was tried first
 * and was not dependable: on Android 13+ it is suppressed unless the app is in the
 * foreground, and this dialog lives in its own window, so pressing Copy looked like it
 * did nothing at all.
 */
    private fun copyToClipboard(text: String): Boolean {
        val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as? android.content.ClipboardManager
            ?: return false
        return try {
            clipboard.setPrimaryClip(android.content.ClipData.newPlainText(TAG, text))
            true
        } catch (e: Exception) {
            Log.w(TAG, "Clipboard write failed", e)
            false
        }
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

    /** Launches a VXP file directly via nativeLoadVxpFile on a background worker. */
    private fun launchVxp(file: File) {
        if (!file.exists()) {
            Toast.makeText(this, "File does not exist: ${file.name}", Toast.LENGTH_SHORT).show()
            return
        }
        VxpRepository(this).addRecentPath(file.absolutePath)
        Thread {
            try {
                nativeLoadVxpFile(file.absolutePath)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to load VXP file: ${file.absolutePath}", e)
                runOnUiThread {
                    Toast.makeText(this, "Failed to load VXP", Toast.LENGTH_SHORT).show()
                }
            }
        }.start()
    }

    /** Exports or shares a VXP file using Android's system share sheet via FileProvider. */
    private fun shareVxp(file: File) {
        if (!file.exists()) {
            Toast.makeText(this, "File does not exist: ${file.name}", Toast.LENGTH_SHORT).show()
            return
        }
        try {
            val contentUri = androidx.core.content.FileProvider.getUriForFile(
                this,
                "$packageName.fileprovider",
                file
            )
            val shareIntent = Intent(Intent.ACTION_SEND).apply {
                type = "application/octet-stream"
                putExtra(Intent.EXTRA_STREAM, contentUri)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            startActivity(Intent.createChooser(shareIntent, "Share ${file.name}"))
        } catch (e: Exception) {
            Log.e(TAG, "Failed to share VXP file", e)
            Toast.makeText(this, "Could not share file", Toast.LENGTH_SHORT).show()
        }
    }

    /** Confirmation now happens on the Hard reset sheet page, so this only runs the reset. */
    private fun performHardReset() {
        requestHardReset()
        Toast.makeText(this, "Reset emulator state requested", Toast.LENGTH_SHORT).show()
    }

    /** Confirmation now happens on the Clear user data sheet page, so this only clears. */
    private fun performClearUserData() {
        // Walks and unlinks the whole guest fs/c and fs/d trees. On a device with a
        // few thousand emulated files that is seconds of work, so it must not run on
        // the main thread: it would ANR with no UI left on screen, because the sheet
        // has already been dismissed by the time we get here.
        Thread {
            var removed = 0
            var missing = false
            var errored = false
            for (drive in arrayOf("c", "d")) {
                val root = File("/sdcard/MREmu/fs/$drive")
                if (!root.exists()) {
                    // Nothing to clear is not the same as cleared successfully, and
                    // reporting it as success hides a denied MANAGE_EXTERNAL_STORAGE.
                    missing = true
                    continue
                }
                try {
                    root.walkBottomUp().forEach { entry ->
                        if (entry != root && entry.isFile && entry.delete()) removed++
                    }
                } catch (e: Exception) {
                    errored = true
                    Log.w(TAG, "Clearing drive $drive failed", e)
                }
            }
            val message = when {
                (missing || errored) && removed == 0 -> "Nothing was cleared"
                errored -> "Cleared user data ($removed files, some failed)"
                missing -> "Cleared user data ($removed files)"
                else -> "Cleared user data ($removed files)"
            }
            runOnUiThread { Toast.makeText(this, message, Toast.LENGTH_SHORT).show() }
        }.start()
    }

    private var cachedVersion: String? = null

    /**
     * Cached because it is a PackageManager binder round-trip and it is read from a
     * composable body, so an unmemoised version repeated it on every recomposition.
     */
    private fun appVersion(): String {
        cachedVersion?.let { return it }
        return try {
            packageManager.getPackageInfo(packageName, 0).versionName.orEmpty()
                .also { cachedVersion = it }
        } catch (e: Exception) {
            ""
        }
    }

private enum class OptionsPage(val title: String) {
        Main("Options"),
        VxpLibrary("VXP Library"),
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
        stack: PageBackStack<OptionsPage>,
        onLaunchVxp: (File) -> Unit,
        onShareVxp: (File) -> Unit,
        onHardReset: () -> Unit,
        onOpenFolder: (File) -> Unit,
        onClearUserData: () -> Unit,
        onAbout: () -> Unit,
        onQuit: () -> Unit,
        onOpenVxpBrowser: () -> Unit,
        onImportVxp: () -> Unit,
        onDismiss: () -> Unit
    ) {
        val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
        var showSheet by remember { mutableStateOf(true) }
        // Guards re-entry: several rows can reach dismissSheet, and hide() is a suspend
        // call that would otherwise be launched twice for the same dismissal.
        var dismissing by remember { mutableStateOf(false) }
        var libraryMenuOpen by remember { mutableStateOf(false) }
        val page = stack.current
        val sheetScope = rememberCoroutineScope()

        // Clearing showSheet drops ModalBottomSheet out of composition on the very next
        // frame, so the sheet vanished with no exit animation at all. hide() has to run
        // while the sheet is still composed: it plays the slide-down, and only once it
        // has finished is it safe to remove it. The action the row was invoking runs
        // last, so the UI settles before the app navigates away or the window goes.
        fun dismissSheet(then: () -> Unit = {}) {
            if (dismissing || !showSheet) return
            dismissing = true
            sheetScope.launch {
                sheetState.hide()
                showSheet = false
                onDismiss()
                then()
            }
        }

        if (showSheet) {
            ModalBottomSheet(
                onDismissRequest = { dismissSheet() },
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
                                else dismissSheet()
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

                        // Page-level actions on the right. Only the VXP library has one
                        // for now; the other pages leave the slot empty rather than
                        // reserving space for an icon that is not there.
                        if (page == OptionsPage.VxpLibrary) {
                            Box(modifier = Modifier.align(Alignment.CenterEnd)) {
                                IconButton(onClick = { libraryMenuOpen = true }) {
                                    Icon(
                                        imageVector = Icons.Filled.MoreVert,
                                        contentDescription = "VXP library options",
                                        tint = Color(0xFFC7C5D0)
                                    )
                                }

                                DropdownMenu(
                                    expanded = libraryMenuOpen,
                                    onDismissRequest = { libraryMenuOpen = false },
                                    containerColor = Color(0xFF2B2B30),
                                    shape = RoundedCornerShape(16.dp),
                                    shadowElevation = 12.dp,
                                    tonalElevation = 0.dp
                                ) {
                                    DropdownMenuItem(
                                        text = {
                                            Text(
                                                text = stringResource(R.string.vxp_browser),
                                                style = MaterialTheme.typography.titleMedium,
                                                color = Color(0xFFE6E1E5)
                                            )
                                        },
                                        leadingIcon = {
                                            Icon(
                                                imageVector = Icons.Filled.Language,
                                                contentDescription = null,
                                                tint = Color(0xFFE6E1E5),
                                                modifier = Modifier.size(24.dp)
                                            )
                                        },
                                        onClick = {
                                            libraryMenuOpen = false
                                            onOpenVxpBrowser()
                                        },
                                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 8.dp)
                                    )
                                    DropdownMenuItem(
                                        text = {
                                            Text(
                                                text = stringResource(R.string.vxp_import),
                                                style = MaterialTheme.typography.titleMedium,
                                                color = Color(0xFFE6E1E5)
                                            )
                                        },
                                        leadingIcon = {
                                            Icon(
                                                imageVector = Icons.Filled.FileOpen,
                                                contentDescription = null,
                                                tint = Color(0xFFE6E1E5),
                                                modifier = Modifier.size(24.dp)
                                            )
                                        },
                                        onClick = {
                                            libraryMenuOpen = false
                                            onImportVxp()
                                        },
                                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 8.dp)
                                    )
                                }
                            }
                        }
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
                                            Icons.Filled.SportsEsports,
                                            stringResource(R.string.section_vxp_library),
                                            Icons.AutoMirrored.Filled.KeyboardArrowRight,
                                            divider = true
                                        ) { stack.push(OptionsPage.VxpLibrary) }
                                        OptionRow(
                                            Icons.Filled.FolderOpen,
                                            "Open FS folder...",
                                            Icons.AutoMirrored.Filled.KeyboardArrowRight,
                                            divider = false
                                        ) { stack.push(OptionsPage.Folders) }
                                    }
                                    SegmentHeader(stringResource(R.string.section_app))
                                    SegmentGroup {
                                        OptionRow(
                                            Icons.Filled.Info,
                                            "About",
                                            Icons.AutoMirrored.Filled.KeyboardArrowRight
                                        ) {
                                            stack.push(OptionsPage.About)
                                        }
                                        OptionRow(
                                            Icons.Filled.PowerSettingsNew,
                                            "Quit",
                                            tint = Color(0xFFE98A8A),
                                            divider = false
                                        ) {
                                            dismissSheet(onQuit)
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
                                                dismissSheet { onOpenFolder(File(drive.path)) }
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
                                                dismissSheet { onOpenFolder(File(drive.path)) }
                                            }
                                        }
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }

                            OptionsPage.HardReset -> {
                                Column(Modifier.fillMaxWidth()) {
                                    Text(
                                        text = stringResource(R.string.hard_reset_message),
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = Color(0xFFC7C5D0),
                                        modifier = Modifier.padding(horizontal = 24.dp, vertical = 12.dp)
                                    )
                                    // No countdown here. Emulator state is disposable and
                                    // rebuilt immediately, so a three second lockout was
                                    // pure friction in front of an action that costs
                                    // nothing. Clear user data keeps its countdown, since
                                    // that one permanently deletes files the guest wrote.
                                    DestructiveButton("Reset", null) {
                                        dismissSheet(onHardReset)
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
                                        dismissSheet(onClearUserData)
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

                            OptionsPage.VxpLibrary -> {
                                VxpLibraryPage(
                                    onLaunch = { item ->
                                        dismissSheet { onLaunchVxp(item.file) }
                                    },
                                    onShare = { item ->
                                        onShareVxp(item.file)
                                    },
                                    onRevealFolder = { folder ->
                                        dismissSheet { onOpenFolder(folder) }
                                    }
                                )
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
     * A destructive confirm button.
     *
     * When [remaining] is non-null the button counts down from 3..1 as soon as the page it
     * sits on is opened, showing just the number until it reaches zero, and stays disabled
     * until then. That replaces a separate confirmation checkbox. Passing null skips the
     * countdown entirely and the button is live immediately.
     *
     * The countdown deliberately starts on navigation rather than on the first tap -- that
     * is the intended design, and the earlier "on the first tap" wording in this comment was
     * simply wrong about what the code does.
     */
    @Composable
    private fun DestructiveButton(label: String, remaining: Int?, onConfirmed: () -> Unit) {
        val red = Color(0xFFC62828)
        val counting = remaining != null && remaining > 0
        Button(
            onClick = onConfirmed,
            enabled = !counting,
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
                text = if (counting) "${remaining}s" else label,
                style = MaterialTheme.typography.bodyLarge
            )
        }
    }

    /**
     * Fatal-error dialog raised by the emulator.
     *
     * This is the Android counterpart of the desktop ImGui "VXP Error" popup: the message
     * is the same text, but it is laid out as a Compose dialog so it matches the rest of
     * the app's UI instead of a native overlay. The body scrolls and wraps because the
     * message carries a full path and can be far longer than one screen.
     */
    @Composable
    private fun ErrorDialog(
        message: String,
        onCopy: () -> Boolean,
        onDismiss: () -> Unit
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clickable(onClick = onDismiss)
                .padding(24.dp),
            contentAlignment = Alignment.Center
        ) {
            Surface(
                shape = RoundedCornerShape(24.dp),
                color = Color(0xFF232327),
                modifier = Modifier
                    .fillMaxWidth()
                    // Taps inside the card must not dismiss; only the scrim behind it does.
                    .clickable(enabled = false) {}
            ) {
                Column(Modifier.padding(24.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Filled.ErrorOutline,
                            contentDescription = null,
                            tint = Color(0xFFE98A8A),
                            modifier = Modifier.size(22.dp)
                        )
                        Spacer(Modifier.size(12.dp))
                        Text(
                            text = stringResource(R.string.error_title),
                            style = MaterialTheme.typography.titleSmall,
                            color = Color.White
                        )
                    }

                    Spacer(Modifier.height(16.dp))

                    // Bounded so a long path scrolls inside the dialog instead of
                    // pushing the buttons off screen.
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = 220.dp)
                            .clip(RoundedCornerShape(14.dp))
                            .background(Color(0xFF1B1B1F))
                            .border(1.dp, Color(0xFF34343A), RoundedCornerShape(14.dp))
                            .verticalScroll(rememberScrollState())
                            .padding(12.dp)
                    ) {
                        Text(
                            text = message,
                            style = MaterialTheme.typography.bodySmall,
                            color = Color(0xFFC7C5D0)
                        )
                    }

                    Spacer(Modifier.height(20.dp))

                    // Confirmation lives in the dialog. A toast was not dependable
                    // here: this is a separate window, so the system's own clipboard
                    // confirmation is frequently suppressed and the button looked dead.
                    var copied by remember { mutableStateOf(false) }
                    LaunchedEffect(copied) {
                        if (copied) {
                            kotlinx.coroutines.delay(1500)
                            copied = false
                        }
                    }

                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(16.dp))
                            .background(Color(0xFF2F2F36))
                            .border(1.dp, Color(0xFF34343A), RoundedCornerShape(16.dp))
                    ) {
                        ErrorDialogAction(
                            label = stringResource(
                                if (copied) R.string.copied_to_clipboard else R.string.copy
                            ),
                            icon = if (copied) Icons.Filled.Check else Icons.Filled.ContentCopy,
                            onClick = {
                                if (onCopy()) copied = true
                            }
                        )
                        HorizontalDivider(color = Color(0xFF34343A))
                        ErrorDialogAction(stringResource(R.string.ok), primary = true, onClick = onDismiss)
                    }
                }
            }
        }
    }

    /** One full-width row of [ErrorDialog]'s stacked actions. */
    @Composable
    private fun ErrorDialogAction(
        label: String,
        onClick: () -> Unit,
        primary: Boolean = false,
        icon: androidx.compose.ui.graphics.vector.ImageVector? = null
    ) {
        Surface(
            onClick = onClick,
            modifier = Modifier.fillMaxWidth(),
            color = Color.Transparent
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 16.dp),
                contentAlignment = Alignment.Center
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    if (icon != null) {
                        Icon(
                            imageVector = icon,
                            contentDescription = null,
                            tint = if (primary) Color.White else Color(0xFFC7C5D0),
                            modifier = Modifier.size(18.dp)
                        )
                    }
                    Text(
                        text = label,
                        style = MaterialTheme.typography.bodyLarge,
                        fontWeight = if (primary) FontWeight.SemiBold else FontWeight.Normal,
                        color = if (primary) Color.White else Color(0xFFC7C5D0)
                    )
                }
            }
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

    override fun onStart() {
        super.onStart()
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START)
    }

    override fun onResume() {
        super.onResume()
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
    }

    override fun onPause() {
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_PAUSE)
        super.onPause()
    }

    override fun onStop() {
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)
        super.onStop()
    }

    override fun onDestroy() {
        // This registry used to be left parked in STARTED forever: there was no
        // onStart, no onStop and no onDestroy, so it never reached DESTROYED and
        // the ViewModelStore was never cleared. Anything registering a lifecycle
        // observer through this activity kept posting work after it was gone, and
        // the options dialog leaked its window.
        optionsDialog?.dismiss()
        optionsDialog = null
        errorDialog?.dismiss()
        errorDialog = null
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_DESTROY)
        viewModelStoreImpl.clear()
        super.onDestroy()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        savedStateRegistryController.performSave(outState)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        // Without this, getIntent() keeps returning the original VIEW intent.
        setIntent(intent)
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
        } else if (requestCode == PICK_VXP_REQUEST_CODE) {
            if (resultCode != Activity.RESULT_OK) return
            val uri = data?.data ?: return
            // Persist the read grant so a later recreation can still reopen the document.
            try {
                contentResolver.takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
            } catch (e: SecurityException) {
                Log.w("MREmu", "Could not persist read grant for $uri", e)
            }
            importVxpUri(uri)
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
        if (intent?.action != Intent.ACTION_VIEW) return
        val uri = intent.data ?: return

        // Consume it. getIntent() keeps returning this VIEW intent after
        // onNewIntent unless it is replaced, so every later recreation (dark mode,
        // font scale, density, or process death) would re-copy and relaunch the
        // originally-opened app.
        intent.action = null

        importVxpUri(uri)
    }

    /**
     * Copies a picked .vxp into the cache, records it as recent, and launches it.
     *
     * Shared by the VIEW intent and the library's "Import VXP..." menu item, so both
     * routes behave identically.
     */
    private fun importVxpUri(uri: Uri) {
        Thread {
            try {
                val input = contentResolver.openInputStream(uri)
                if (input == null) {
                    // Document providers return null for a deleted document or a
                    // revoked grant. Handing the path to native anyway booted a
                    // 0-byte image with no visible error.
                    Log.w("MREmu", "Could not open $uri")
                    runOnUiThread {
                        Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT).show()
                    }
                    return@Thread
                }

                // Unique per load: the fixed "opened_app.vxp" name meant two VIEW
                // deliveries wrote the same file concurrently and interleaved into a
                // corrupt image, because FileOutputStream truncates on open.
                val tempVxpFile = File.createTempFile("opened_", ".vxp", cacheDir)

                input.use { src ->
                    FileOutputStream(tempVxpFile).use { src.copyTo(it) }
                }

                val realPosixPath = tempVxpFile.absolutePath
                Log.d("MREmu", "File copied to cache: $realPosixPath")
                VxpRepository(this).addRecentPath(realPosixPath)

                // Deliberately NOT wrapped in runOnUiThread. nativeLoadVxpFile waits
                // for g_appManager, which the render thread only publishes part way
                // through start-up, so calling it on the UI thread blocked the main
                // thread in a sleep loop for the whole of a cold start.
                nativeLoadVxpFile(realPosixPath)
            } catch (e: Exception) {
                Log.e("MREmu", "Copy to cache error", e)
                runOnUiThread {
                    Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT).show()
                }
            }
        }.start()
    }

    /** Opens the system file picker so a .vxp can be added to the library. */
    private fun openVxpPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            // Feature phones have no registered MIME type for .vxp, and several
            // document providers report them as octet-stream or with no type at all,
            // so filter by extension rather than trusting a MIME type.
            type = "*/*"
            putExtra(Intent.EXTRA_MIME_TYPES, arrayOf("application/octet-stream", "application/zip"))
        }
        try {
            startActivityForResult(intent, PICK_VXP_REQUEST_CODE)
        } catch (e: ActivityNotFoundException) {
            Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT).show()
        }
    }

    

    /**
     * Plays a predefined haptic pattern for the Options button.
     *
     * Uses [View.performHapticFeedback] rather than the vibrator: the platform picks the
     * right waveform for the attached input device, so it reads as a UI tick rather than
     * the generic buzz [triggerVibration] produces for emulated key presses. Unknown
     * constants are ignored, and a device with no haptics simply does nothing.
     *
     * Called from the native render thread, so it has to hop to the UI thread: haptic
     * feedback is dispatched through the view's handler.
     */
    fun triggerHapticFeedback(feedbackConstant: Int) {
        runOnUiThread {
            try {
                window.decorView.performHapticFeedback(feedbackConstant)
            } catch (e: Exception) {
                Log.w(TAG, "Haptic feedback unavailable", e)
            }
        }
    }

    /**
     * Plays [durationMs] of vibration, shaped for the actuator this handset actually has.
     *
     * Phones ship two very different motors, and a duration that suits one is wrong for the
     * other. A linear resonant actuator is crisp and resolves a 10-20 ms tick perfectly. An
     * eccentric rotating mass motor - the coin motor still fitted to a lot of budget Android
     * phones - needs roughly 15-30 ms just to spin the mass past the perceptible threshold,
     * so a 40 ms one-shot arrives barely started and reads as almost nothing. Retriggered
     * faster than that, which is exactly what the app selector's key auto-repeat does once it
     * accelerates to 50 ms, the mass is cancelled mid spin-up over and over and the result
     * is a stutter rather than a buzz.
     *
     * No public API reports motor type. The predefined effects are the way out of that:
     * each OEM tunes them against the hardware in the handset, so asking for EFFECT_TICK
     * lands a waveform that suits whichever motor is fitted. API 29 and up use them for
     * short pulses. A long pulse is a sustained buzz rather than a tick and no predefined
     * effect covers that, so it stays a one-shot of the requested length - a sustained buzz
     * is motor-agnostic anyway. Below API 29 there are no predefined effects at all, so the
     * duration is lifted to the rotating-mass floor instead: that makes the tick a little
     * long on a linear actuator, but it is the only way it is felt on the cheap motor.
     *
     * Called from the native render thread.
     */
    fun triggerVibration(durationMs: Long) {
        try {
            val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vibratorManager = getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
                vibratorManager?.defaultVibrator
            } else {
                @Suppress("DEPRECATION")
                getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
            } ?: return

            if (durationMs <= 0) {
                vibrator.cancel()
                return
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && durationMs <= PREDEFINED_MAX_MS) {
                // EFFECT_TICK / EFFECT_CLICK are effect ids, not VibrationEffect
                // objects; createPredefined() is what turns one into a waveform.
                val effectId = if (durationMs <= TICK_MAX_MS) VibrationEffect.EFFECT_TICK
                else VibrationEffect.EFFECT_CLICK
                vibrator.vibrate(VibrationEffect.createPredefined(effectId))
                return
            }

            val effectiveMs =
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) durationMs
                else maxOf(durationMs, ROTATING_MASS_FLOOR_MS)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                vibrator.vibrate(VibrationEffect.createOneShot(effectiveMs, VibrationEffect.DEFAULT_AMPLITUDE))
            } else {
                @Suppress("DEPRECATION")
                vibrator.vibrate(effectiveMs)
            }
        } catch (e: Exception) {
            Log.w(TAG, "Vibration failed", e)
        }
    }

    fun hasVibrator(): Boolean {
        return try {
            val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vibratorManager = getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
                vibratorManager?.defaultVibrator
            } else {
                @Suppress("DEPRECATION")
                getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
            }
            vibrator?.hasVibrator() ?: false
        } catch (e: Exception) {
            false
        }
    }
}
