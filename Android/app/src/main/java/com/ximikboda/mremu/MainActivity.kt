package com.ximikboda.mremu

import android.Manifest
import android.app.NativeActivity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
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
import androidx.compose.animation.core.tween
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.CheckBox
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.SdStorage
import androidx.compose.material.icons.outlined.CheckBoxOutlineBlank
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.lifecycle.setViewTreeViewModelStoreOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import java.io.File
import java.io.FileOutputStream

class MainActivity : NativeActivity(), LifecycleOwner, ViewModelStoreOwner, SavedStateRegistryOwner {
    private val lifecycleRegistry = LifecycleRegistry(this)
    private val viewModelStoreImpl = ViewModelStore()
    private val savedStateRegistryController = SavedStateRegistryController.create(this)

    override val lifecycle: Lifecycle get() = lifecycleRegistry
    override val viewModelStore: ViewModelStore get() = viewModelStoreImpl
    override val savedStateRegistry: SavedStateRegistry get() = savedStateRegistryController.savedStateRegistry
    companion object {
        private const val STORAGE_PERMISSION_CODE = 100
        private const val MANAGE_STORAGE_REQUEST_CODE = 101

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
                MaterialTheme {
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

    private fun openFolder(dir: File) {
        if (!dir.exists()) dir.mkdirs()
        try {
            startActivity(Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(Uri.fromFile(dir), "resource/folder")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            })
        } catch (e: Exception) {
            try {
                startActivity(Intent(Intent.ACTION_VIEW).setDataAndType(Uri.fromFile(dir), "*/*"))
            } catch (e2: Exception) {
                Toast.makeText(this, "No app can open ${dir.absolutePath}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    /** Confirmation now happens on the Hard reset sheet page, so this only runs the reset. */
    private fun performHardReset() {
        requestHardReset()
        Toast.makeText(this, "Hard reset requested", Toast.LENGTH_SHORT).show()
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
        HardReset("Hard reset"),
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
        val sheetState = rememberModalBottomSheetState()
        var showSheet by remember { mutableStateOf(true) }
        val stack = rememberPageBackStack(OptionsPage.Main)
        val page = stack.current

        if (showSheet) {
            ModalBottomSheet(
                onDismissRequest = { showSheet = false; onDismiss() },
                sheetState = sheetState,
                containerColor = Color(0xFF1B1B1F),
                contentColor = Color.White
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
                            // Forward = deeper in the stack (slide in from the right),
                            // otherwise we are going back (slide in from the left).
                            // No fade: a pure horizontal slide with an animated height.
                            val goingForward = targetState.ordinal > initialState.ordinal
                            val enter = if (goingForward)
                                slideInHorizontally(tween(300)) { it / 3 }
                            else
                                slideInHorizontally(tween(300)) { -it / 3 }
                            val exit = if (goingForward)
                                slideOutHorizontally(tween(300)) { -it / 3 }
                            else
                                slideOutHorizontally(tween(300)) { it / 3 }
                            (enter togetherWith exit).using(SizeTransform(clip = false))
                        },
                        label = "optionsPage"
                    ) { p ->
                        when (p) {
                            OptionsPage.Main -> {
                                Column(Modifier.fillMaxWidth()) {
                                    OptionRow(Icons.Filled.Refresh, "Hard reset") {
                                        stack.push(OptionsPage.HardReset)
                                    }
                                    OptionRow(
                                        Icons.Filled.FolderOpen,
                                        "Open FS folder...",
                                        Icons.AutoMirrored.Filled.KeyboardArrowRight
                                    ) { stack.push(OptionsPage.Folders) }
                                    OptionRow(Icons.Filled.DeleteSweep, "Clear user data") {
                                        stack.push(OptionsPage.ClearUserData)
                                    }
                                    OptionRow(Icons.Filled.Info, "About") {
                                        stack.push(OptionsPage.About)
                                    }
                                    OptionRow(Icons.Filled.PowerSettingsNew, "Quit", tint = Color(0xFFE98A8A)) {
                                        showSheet = false; onQuit()
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }

                            OptionsPage.Folders -> {
                                Column(Modifier.fillMaxWidth()) {
                                    drives.forEach { drive ->
                                        OptionRow(
                                            icon = if (drive.label.endsWith("mre"))
                                                Icons.Filled.Folder else Icons.Filled.SdStorage,
                                            label = drive.label,
                                            detail = drive.detail,
                                            trailing = Icons.AutoMirrored.Filled.KeyboardArrowRight
                                        ) {
                                            showSheet = false
                                            onOpenFolder(File(drive.path))
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
                                        modifier = Modifier.padding(horizontal = 24.dp, vertical = 8.dp)
                                    )
                                    var armed by remember { mutableStateOf(false) }
                                    CheckRow("Reset the emulator now", armed) { armed = !armed }
                                    Button(
                                        onClick = { showSheet = false; onHardReset() },
                                        enabled = armed,
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .padding(horizontal = 24.dp, vertical = 8.dp)
                                    ) {
                                        Text(stringResource(R.string.hard_reset_confirm))
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }

                            OptionsPage.ClearUserData -> {
                                Column(Modifier.fillMaxWidth()) {
                                    Text(
                                        text = stringResource(R.string.clear_data_message),
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = Color(0xFFC7C5D0),
                                        modifier = Modifier.padding(horizontal = 24.dp, vertical = 8.dp)
                                    )
                                    var armed by remember { mutableStateOf(false) }
                                    CheckRow("Erase everything on fs/c and fs/d", armed) { armed = !armed }
                                    Button(
                                        onClick = { showSheet = false; onClearUserData() },
                                        enabled = armed,
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .padding(horizontal = 24.dp, vertical = 8.dp)
                                    ) {
                                        Text(stringResource(R.string.clear_data_confirm))
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }

                            OptionsPage.About -> {
                                Column(Modifier.fillMaxWidth()) {
                                    Text(
                                        text = stringResource(R.string.about_message),
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = Color(0xFFC7C5D0),
                                        modifier = Modifier.padding(horizontal = 24.dp, vertical = 8.dp)
                                    )
                                    OptionRow(Icons.Filled.SdStorage, "VXP folder", detail = "/sdcard/MREmu/fs/e/mre") {
                                        showSheet = false
                                        onOpenFolder(File("/sdcard/MREmu/fs/e/mre"))
                                    }
                                    OptionRow(
                                        Icons.Filled.Info,
                                        "Version",
                                        detail = appVersion().ifEmpty { "-" },
                                        onClick = {}
                                    )
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
        onClick: () -> Unit
    ) {
        Surface(
            onClick = onClick,
            modifier = Modifier.fillMaxWidth(),
            color = Color.Transparent
        ) {
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
        }
    }

    /**
     * A destructive action armed by a todo-list checkbox: the row reads as a task to
     * tick off, and the enabled state of the confirm button follows the tick.
     */
    @Composable
    private fun CheckRow(label: String, checked: Boolean, onCheckedChange: (Boolean) -> Unit) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clickable { onCheckedChange(!checked) }
                .padding(horizontal = 24.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(20.dp)
        ) {
            Icon(
                imageVector = if (checked) Icons.Filled.CheckBox else Icons.Outlined.CheckBoxOutlineBlank,
                contentDescription = null,
                tint = if (checked) Color(0xFF8AB4F8) else Color(0xFF8E8E93),
                modifier = Modifier.size(22.dp)
            )
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
                color = if (checked) Color(0xFFE6E1E5) else Color(0xFFC7C5D0),
                modifier = Modifier.weight(1f)
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
