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
import java.io.File
import java.io.FileOutputStream

class MainActivity : NativeActivity() {
    companion object {
        private const val STORAGE_PERMISSION_CODE = 100
        private const val MANAGE_STORAGE_REQUEST_CODE = 101

        init {
            System.loadLibrary("MREmu")
        }
    }

    private external fun notifyPermissionState(granted: Boolean)
    private external fun nativeLoadVxpFile(path: String)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        checkAndRequestStoragePermission()

        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        handleIntent(intent)
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