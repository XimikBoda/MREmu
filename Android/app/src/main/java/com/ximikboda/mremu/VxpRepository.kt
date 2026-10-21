package com.ximikboda.mremu

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

data class VxpItem(
    val file: File,
    val isRecent: Boolean = false,
    val name: String = file.name,
    val path: String = file.absolutePath,
    val sizeBytes: Long = if (file.exists()) file.length() else 0L,
    val lastModified: Long = if (file.exists()) file.lastModified() else 0L,
    val formattedSize: String = formatBytes(if (file.exists()) file.length() else 0L),
    val formattedDate: String = formatDate(if (file.exists()) file.lastModified() else 0L)
) {
    companion object {
        private fun formatBytes(bytes: Long): String = when {
            bytes < 1024 -> "$bytes B"
            bytes < 1024 * 1024 -> String.format(Locale.US, "%.1f KB", bytes / 1024.0)
            else -> String.format(Locale.US, "%.2f MB", bytes / (1024.0 * 1024.0))
        }

        private fun formatDate(time: Long): String {
            if (time <= 0) return "-"
            val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault())
            return sdf.format(Date(time))
        }
    }
}

class VxpRepository(context: Context) {
    private val prefs: SharedPreferences =
        context.getSharedPreferences("mremu_vxp_library", Context.MODE_PRIVATE)

    companion object {
        private const val KEY_RECENTS = "recent_vxp_paths"
        private const val MAX_RECENTS = 20
        const val GUEST_MRE_DIR = "/sdcard/MREmu/fs/e/mre"
    }

    fun getRecentPaths(): List<String> {
        val raw = prefs.getString(KEY_RECENTS, null) ?: return emptyList()
        return try {
            val arr = JSONArray(raw)
            val list = mutableListOf<String>()
            for (i in 0 until arr.length()) {
                val path = arr.getString(i)
                if (File(path).exists()) {
                    list.add(path)
                }
            }
            list
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun addRecentPath(path: String) {
        val current = getRecentPaths().toMutableList()
        current.remove(path)
        current.add(0, path)
        if (current.size > MAX_RECENTS) {
            current.subList(MAX_RECENTS, current.size).clear()
        }
        val arr = JSONArray(current)
        prefs.edit().putString(KEY_RECENTS, arr.toString()).apply()
    }

    fun removeRecentPath(path: String) {
        val current = getRecentPaths().toMutableList()
        if (current.remove(path)) {
            val arr = JSONArray(current)
            prefs.edit().putString(KEY_RECENTS, arr.toString()).apply()
        }
    }

    fun loadAllVxps(): List<VxpItem> {
        val itemsMap = LinkedHashMap<String, VxpItem>()
        val recentPaths = getRecentPaths().toSet()

        // 1. Scan guest MRE directory
        val mreDir = File(GUEST_MRE_DIR)
        if (mreDir.exists() && mreDir.isDirectory) {
            val files = mreDir.listFiles { f -> f.isFile && f.name.endsWith(".vxp", ignoreCase = true) }
            files?.forEach { file ->
                itemsMap[file.absolutePath] = VxpItem(file = file, isRecent = recentPaths.contains(file.absolutePath))
            }
        }

        // 2. Scan recents (may include external or cache files)
        for (recentPath in recentPaths) {
            val file = File(recentPath)
            if (file.exists() && !itemsMap.containsKey(file.absolutePath)) {
                itemsMap[file.absolutePath] = VxpItem(file = file, isRecent = true)
            }
        }

        return itemsMap.values.toList()
    }

    fun deleteVxp(item: VxpItem): Boolean {
        removeRecentPath(item.path)
        return try {
            if (item.file.exists()) item.file.delete() else true
        } catch (e: Exception) {
            false
        }
    }

    fun renameVxp(item: VxpItem, newName: String): File? {
        val cleanName = if (newName.endsWith(".vxp", ignoreCase = true)) newName else "$newName.vxp"
        val parent = item.file.parentFile ?: return null
        val target = File(parent, cleanName)
        if (target.exists()) return null
        return try {
            if (item.file.renameTo(target)) {
                if (item.isRecent) {
                    removeRecentPath(item.path)
                    addRecentPath(target.absolutePath)
                }
                target
            } else {
                null
            }
        } catch (e: Exception) {
            null
        }
    }
}
