package com.ximikboda.mremu

import android.Manifest
import android.app.AlertDialog
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
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
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
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.SdStorage
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
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
import androidx.compose.ui.platform.ViewCompositionStrategy
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
                        onHardReset = { dialog.dismiss(); confirmHardReset() },
                        onOpenFolder = { dir -> dialog.dismiss(); openFolder(dir) },
                        onClearUserData = { dialog.dismiss(); confirmClearUserData() },
                        onAbout = { dialog.dismiss(); showAbout() },
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

    /** Sub-dialog listing every emulated drive, so a folder can be opened directly. */
    private fun presentFsFolderDialog() {
        val labels = arrayOf("fs/c  (internal)", "fs/d  (internal)", "fs/e  (external)", "fs/e/mre  (VXP)")
        val paths = arrayOf(
            File("/sdcard/MREmu/fs/c"),
            File("/sdcard/MREmu/fs/d"),
            File("/sdcard/MREmu/fs/e"),
            File("/sdcard/MREmu/fs/e/mre")
        )

        AlertDialog.Builder(this)
            .setTitle(R.string.open_fs_folder)
            .setItems(labels) { _, which -> openFolder(paths[which]) }
            .setNegativeButton(R.string.cancel, null)
            .show()
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

    private fun confirmHardReset() {
        AlertDialog.Builder(this)
            .setTitle(R.string.hard_reset_title)
            .setMessage(R.string.hard_reset_message)
            .setNegativeButton(R.string.cancel, null)
            .setPositiveButton(R.string.hard_reset_confirm) { _, _ ->
                requestHardReset()
                Toast.makeText(this, "Hard reset requested", Toast.LENGTH_SHORT).show()
            }
            .show()
    }

    private fun confirmClearUserData() {
        AlertDialog.Builder(this)
            .setTitle(R.string.clear_data_title)
            .setMessage(R.string.clear_data_message)
            .setNegativeButton(R.string.cancel, null)
            .setPositiveButton(R.string.clear_data_confirm) { _, _ ->
                var removed = 0
                for (drive in arrayOf("c", "d")) {
                    val root = File("/sdcard/MREmu/fs/$drive")
                    root.walkBottomUp().forEach { entry ->
                        if (entry != root && entry.isFile && entry.delete()) removed++
                    }
                }
                Toast.makeText(this, "Cleared user data ($removed files)", Toast.LENGTH_SHORT).show()
            }
            .show()
    }

    private fun showAbout() {
        AlertDialog.Builder(this)
            .setTitle(R.string.about_title)
            .setMessage(R.string.about_message)
            .setPositiveButton(android.R.string.ok, null)
            .show()
    }

    private enum class OptionsPage { Main, Folders }

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
        var page by remember { mutableStateOf(OptionsPage.Main) }

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
                                if (page == OptionsPage.Folders) page = OptionsPage.Main
                                else { showSheet = false; onDismiss() }
                            },
                            modifier = Modifier.align(Alignment.CenterStart)
                        ) {
                            Icon(
                                imageVector = if (page == OptionsPage.Folders)
                                    Icons.AutoMirrored.Filled.ArrowBack else Icons.Filled.Close,
                                contentDescription = if (page == OptionsPage.Folders) "Back" else "Close",
                                tint = Color(0xFFE6E1E5)
                            )
                        }

                        Text(
                            text = if (page == OptionsPage.Main) "Options" else "Open fs folder",
                            style = MaterialTheme.typography.headlineSmall,
                            fontWeight = FontWeight.Bold,
                            color = Color(0xFFE6E1E5),
                            modifier = Modifier.align(Alignment.Center)
                        )
                    }

                    AnimatedContent(
                        targetState = page,
                        transitionSpec = {
                            val goingForward = targetState == OptionsPage.Folders
                            val enter = if (goingForward)
                                (slideInHorizontally { it / 3 } + fadeIn())
                            else
                                (slideInHorizontally { -it / 3 } + fadeIn())
                            val exit = if (goingForward)
                                (slideOutHorizontally { -it / 3 } + fadeOut())
                            else
                                (slideOutHorizontally { it / 3 } + fadeOut())
                            (enter togetherWith exit).using(SizeTransform(clip = false))
                        },
                        label = "optionsPage"
                    ) { p ->
                        when (p) {
                            OptionsPage.Main -> {
                                Column(Modifier.fillMaxWidth()) {
                                    OptionRow(Icons.Filled.Refresh, "Hard Reset", null) {
                                        showSheet = false; onHardReset()
                                    }
                                    OptionRow(
                                        Icons.Filled.FolderOpen,
                                        "Open fs folder",
                                        Icons.AutoMirrored.Filled.KeyboardArrowRight
                                    ) { page = OptionsPage.Folders }
                                    OptionRow(Icons.Filled.DeleteSweep, "Clear user data", null) {
                                        showSheet = false; onClearUserData()
                                    }
                                    OptionRow(Icons.Filled.Info, "About", null) {
                                        showSheet = false; onAbout()
                                    }
                                    OptionRow(Icons.Filled.PowerSettingsNew, "Quit", null, tint = Color(0xFFE98A8A)) {
                                        showSheet = false; onQuit()
                                    }
                                    Spacer(Modifier.height(16.dp))
                                }
                            }

                            OptionsPage.Folders -> {
                                Column(Modifier.fillMaxWidth()) {
                                    OptionRow(Icons.Filled.SdStorage, "fs/c", Icons.AutoMirrored.Filled.KeyboardArrowRight) {
                                        showSheet = false; onOpenFolder(File("/sdcard/MREmu/fs/c"))
                                    }
                                    OptionRow(Icons.Filled.SdStorage, "fs/d", Icons.AutoMirrored.Filled.KeyboardArrowRight) {
                                        showSheet = false; onOpenFolder(File("/sdcard/MREmu/fs/d"))
                                    }
                                    OptionRow(Icons.Filled.SdStorage, "fs/e", Icons.AutoMirrored.Filled.KeyboardArrowRight) {
                                        showSheet = false; onOpenFolder(File("/sdcard/MREmu/fs/e"))
                                    }
                                    OptionRow(Icons.Filled.Folder, "fs/e/mre", Icons.AutoMirrored.Filled.KeyboardArrowRight) {
                                        showSheet = false; onOpenFolder(File("/sdcard/MREmu/fs/e/mre"))
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

    @Composable
    private fun OptionRow(
        icon: androidx.compose.ui.graphics.vector.ImageVector,
        label: String,
        trailing: androidx.compose.ui.graphics.vector.ImageVector?,
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
                Text(
                    text = label,
                    style = MaterialTheme.typography.bodyLarge,
                    color = tint,
                    modifier = Modifier.weight(1f)
                )
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