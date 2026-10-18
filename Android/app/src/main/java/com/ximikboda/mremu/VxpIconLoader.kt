package com.ximikboda.mremu

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.LruCache
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile

/**
 * Extracts an app icon out of a `.vxp` file without loading or running the app.
 *
 * This deliberately re-implements container parsing in Kotlin rather than asking the
 * engine for the icon. The engine can only read resources of an app it has already
 * loaded, and loading is not always possible: compressed non-ADS packages are rejected
 * by `ArmApp::load_from_file` ("zipped no ads is not realized"), and a handful of files
 * in the library fault before they finish loading. Several of those - App Store,
 * Gameloft, Spider Man - still carry a perfectly good icon, so tying the icon to the
 * loader would lose exactly the apps most worth showing an icon for.
 *
 * Nothing here executes guest code, so a malformed package can at worst fail to
 * produce an icon. Every offset is bounds-checked against the real file length, because
 * a VXP is untrusted input and its header fields are not validated by the producer.
 */
object VxpIconLoader {

    /** Icon bytes wrapped by the MRE "VREAPPLOGO" container. */
    private const val VRE_HEADER_SIZE = 23
    private val VRE_MAGIC = "VREAPPLOGO".toByteArray(Charsets.US_ASCII)

    /** Image signatures recognised inside a VRE-wrapped payload. */
    private val IMAGE_MAGICS = arrayOf(
        byteArrayOf(0x89.toByte(), 'P'.code.toByte(), 'N'.code.toByte(), 'G'.code.toByte()), // PNG
        "GIF8".toByteArray(Charsets.US_ASCII),
        byteArrayOf(0xFF.toByte(), 0xD8.toByte()),                                                 // JPEG
        "BM".toByteArray(Charsets.US_ASCII),                                                        // BMP
        "RIFF".toByteArray(Charsets.US_ASCII)                                                      // WEBP container
    )

    /**
     * How far into a VRE payload to look for the real image.
     *
     * The `JPG` variant stores eight bytes of extra data ahead of a PNG, so the image
     * is not always at offset 0 of the payload. 64 covers every layout seen in the
     * library while keeping a false positive on random data unlikely.
     */
    private const val MAGIC_SEARCH_LIMIT = 64

    private val cache = object : LruCache<String, ImageBitmap>(32) {
        override fun sizeOf(key: String, value: ImageBitmap): Int = 1
    }

    /**
     * Where decoded icons are persisted between runs.
     *
     * The in-memory [cache] dies with the process, so without this every launch
     * re-parses and re-decodes every VXP in the library before the first row can paint.
     * Set once from a context; when null the loader still works, just without the
     * on-disk tier.
     */
    @Volatile
    private var cacheDir: File? = null

    fun init(context: Context) {
        cacheDir = File(context.cacheDir, "vxp_icons").apply { mkdirs() }
    }

    /**
     * Returns the icon for [file], or null when the package has none we can decode.
     *
     * Cache keys include the last-modified time and size so a renamed or replaced VXP
     * does not keep serving the previous file's icon.
     */
    fun load(file: File): ImageBitmap? {
        if (!file.isFile || file.length() < 32) return null
        val key = "${file.absolutePath}:${file.lastModified()}:${file.length()}"
        cache.get(key)?.let { return it }

        val bitmap = try {
            decodeFromDisk(key) ?: decodeAndStore(file, key)
        } catch (e: Exception) {
            null
        }
        return bitmap?.asImageBitmap()?.also { cache.put(key, it) }
    }

    /** Reads a previously stored icon, or null when there is not one. */
    private fun decodeFromDisk(key: String): Bitmap? {
        val f = diskFile(key) ?: return null
        if (!f.isFile) return null
        return BitmapFactory.decodeFile(f.absolutePath)
    }

    private fun decodeAndStore(file: File, key: String): Bitmap? {
        val decoded = decode(file) ?: return null
        val target = diskFile(key)
        if (target != null) {
            try {
                FileOutputStream(target).use { out ->
                    decoded.compress(Bitmap.CompressFormat.PNG, 100, out)
                }
            } catch (e: Exception) {
                // A failed write only costs a re-decode next time, so it is not fatal.
            }
        }
        return decoded
    }

    private fun diskFile(key: String): File? {
        val dir = cacheDir ?: return null
        // The key contains path separators; hash it into a flat filename.
        val name = Integer.toHexString(key.hashCode()) + "-" +
                Integer.toHexString(key.length) + ".png"
        return File(dir, name)
    }

    private fun decode(file: File): Bitmap? {
        RandomAccessFile(file, "r").use { raf ->
            val length = raf.length()
            if (length < 32) return null

            // The trailer's first field points at the tag table.
            raf.seek(length - 12)
            val tail = ByteArray(12)
            raf.readFully(tail)
            val tagsOffset = le32(tail, 0).toLong()
            if (tagsOffset < 0 || tagsOffset + 12 > length) return null

            val header = readAt(raf, 0, minOf(length, 1024L).toInt())

            // Which container we are in is decided by the file's magic, not by a tag.
            // Tag 0x16 is set on essentially every ADS package including plain
            // uncompressed ELFs, so treating it as "is compressed" would send every
            // ELF down the ADS path, where there is no descriptor to read.
            val section = if (looksLikeElf(header)) {
                elfResourceSection(raf, header, length)
            } else {
                adsResourceSection(raf, tagsOffset, length)
            } ?: return null

            val (resOffset, resSize, localOffsets) = section
            val resStart = resOffset.toLong()
            if (resSize <= 0 || resStart < 0 || resStart > length) return null
            if (resStart + resSize > length) return null

            val resources = readResources(raf, resStart, resSize, localOffsets, length)

            // Ranked, not just best: some packages ship a stub AppLogo.img (a 31-byte
            // header with no image behind it) alongside a usable logo.png, so the
            // top-ranked candidate is not always the one that decodes.
            val candidates = resources
                .filter { it.size in 1..MAX_ICON_BYTES }
                .sortedByDescending { iconScore(it.name) }
            for (c in candidates) {
                if (iconScore(c.name) == 0) break
                val bitmap = decodeImage(readAt(raf, c.offset.toLong(), c.size))
                if (bitmap != null) return bitmap
            }
            return null
        }
    }

    /**
     * True when the package is a plain, uncompressed ELF.
     *
     * This is the only reliable way to tell the two ADS layouts apart: the tag that
     * looks like it should say ("is RO/RW zipped") is present on uncompressed ELFs too.
     */
    private fun looksLikeElf(h: ByteArray): Boolean =
        h.size >= 4 && h[0] == 0x7F.toByte() && h[1] == 'E'.code.toByte() &&
                h[2] == 'L'.code.toByte() && h[3] == 'F'.code.toByte()

    /**
     * Locates .vm_res in a compressed ADS package.
     *
     * The descriptor is nine little-endian u32s ending at `tags_offset - 4`, where a
     * u32 holds its own size (36). res_offset and res_size are the last two fields.
     * Offsets inside the table are relative to res_offset rather than the file.
     */
    private fun adsResourceSection(raf: RandomAccessFile, tagsOffset: Long, length: Long):
            Triple<Int, Int, Boolean>? {
        val sizeField = readAt(raf, tagsOffset - 4, 4)
        val infoSize = le32(sizeField, 0)
        if (infoSize != ADS_INFO_SIZE) return null
        val base = tagsOffset - 4 - infoSize
        if (base < 0) return null
        val info = readAt(raf, base, infoSize)
        return Triple(le32(info, 28), le32(info, 32), true)
    }

    /**
     * Locates .vm_res by walking an uncompressed ELF's section header table.
     *
     * Only [header] (the first 1 KB) is guaranteed to be resident; the section header
     * table normally lives at the very end of the file, so it and the section-name
     * string table are read from disk on demand rather than assumed to be in the header.
     */
    private fun elfResourceSection(raf: RandomAccessFile, header: ByteArray, fileLength: Long):
            Triple<Int, Int, Boolean>? {
        if (header.size < 0x34) return null
        if (header[0] != 0x7F.toByte() || header[1] != 'E'.code.toByte() ||
            header[2] != 'L'.code.toByte() || header[3] != 'F'.code.toByte()) return null
        if (header[4] != 1.toByte()) return null        // only 32-bit ELF; EI_CLASS
        if (header[5] != 1.toByte()) return null        // little-endian only; EI_DATA

        val shoff = le32(header, 0x20).toLong()
        val shentsize = le16(header, 0x2E)
        val shnum = le16(header, 0x30)
        val shstrndx = le16(header, 0x32)
        if (shentsize < 40 || shnum == 0 || shstrndx >= shnum) return null
        val tableBytes = shoff + shnum.toLong() * shentsize
        if (shoff < 0 || tableBytes > fileLength) return null

        val table = readAt(raf, shoff, shnum * shentsize)
        if (table.size < shnum * shentsize) return null

        fun entry(i: Int): Int = i * shentsize

        val strEntry = entry(shstrndx)
        if (strEntry + 24 > table.size) return null
        val strOff = le32(table, strEntry + 16).toLong()
        val strSize = le32(table, strEntry + 20)
        if (strOff < 0 || strOff + strSize > fileLength || strSize <= 0) return null
        val strTab = readAt(raf, strOff, strSize)

        for (i in 0 until shnum) {
            val o = entry(i)
            if (o + 24 > table.size) continue
            // sh_name is an offset *into the string table*, not a file offset.
            val nameIdx = le32(table, o)
            if (nameIdx < 0 || nameIdx >= strTab.size) continue
            val end = indexOfByte(strTab, 0, nameIdx)
            if (end < 0) continue
            val name = String(strTab, nameIdx, end - nameIdx, Charsets.US_ASCII)
            if (name == ".vm_res") {
                return Triple(le32(table, o + 16), le32(table, o + 20), false)
            }
        }
        return null
    }

    private data class Res(val name: String, val offset: Int, val size: Int)

    /**
     * Walks the null-terminated name directory at the head of .vm_res.
     *
     * Layout per entry: NUL-terminated name, then u32 offset, then u32 size. The list
     * ends on an empty name. ADS packages store offsets relative to the section start,
     * hence [localOffsets].
     */
    private fun readResources(
        raf: RandomAccessFile,
        resOffset: Long,
        resSize: Int,
        localOffsets: Boolean,
        fileLength: Long
    ): List<Res> {
        val out = ArrayList<Res>()
        val dir = readAt(raf, resOffset, resSize)
        var pos = 0
        while (pos < dir.size) {
            if (dir[pos] == 0.toByte()) break
            val end = indexOfByte(dir, 0, pos)
            if (end < 0) break
            val name = String(dir, pos, end - pos, Charsets.ISO_8859_1)
            pos = end + 1
            if (pos + 8 > dir.size) break
            var off = le32(dir, pos); pos += 4
            val sz = le32(dir, pos); pos += 4
            if (localOffsets) off += resOffset.toInt()
            // Reject entries pointing outside the file rather than reading them.
            if (off < 0 || sz <= 0) continue
            if (off.toLong() + sz > fileLength) continue
            out.add(Res(name, off, sz))
            if (out.size >= MAX_RESOURCES) break
        }
        return out
    }

    /**
     * Ranks a resource name by how likely it is to be the app icon.
     *
     * `AppLogo.img` is the MRE convention and is what the platform launcher reads, so
     * it wins outright. The rest are fallbacks for packages that ship a plain image
     * instead. Anything scoring 0 is ignored, which keeps the engine's own data files
     * (`r.sf`, `bgTable`, fonts, sprite sheets) out of the running.
     */
    private fun iconScore(name: String): Int {
        val n = name.lowercase()
        if (n == "applogo.img") return 100
        if (n.contains("applogo")) return 90
        if (n.startsWith("_vreapplogo")) return 85
        val looksLikeImage = n.endsWith(".img") || n.endsWith(".png") ||
                n.endsWith(".gif") || n.endsWith(".jpg") || n.endsWith(".jpeg") || n.endsWith(".bmp")
        if (!looksLikeImage) return 0
        if (n == "icon.png" || n == "icon.gif" || n == "icon.jpg" || n == "icon.img") return 60
        if (n.contains("logo")) return 50
        if (n.contains("icon")) return 40
        return 0
    }

    /**
     * Decodes image bytes, unwrapping the VRE container first when present.
     *
     * A VRE-wrapped payload does not necessarily begin with an image signature - the
     * `JPG` variant carries eight bytes of padding ahead of a PNG - so the real start is
     * found by scanning the first few bytes rather than trusting the variant tag.
     */
    private fun decodeImage(data: ByteArray): Bitmap? {
        if (data.isEmpty()) return null

        val payload: ByteArray = if (startsWith(data, VRE_MAGIC, 0)) {
            if (data.size <= VRE_HEADER_SIZE) return null
            val declared = le32(data, 19)
            val end = if (declared in 1..data.size - VRE_HEADER_SIZE) {
                VRE_HEADER_SIZE + declared
            } else {
                data.size
            }
            val body = data.copyOfRange(VRE_HEADER_SIZE, end)
            val at = findImageMagic(body) ?: return null
            body.copyOfRange(at, body.size)
        } else {
            data
        }

        if (payload.isEmpty()) return null
        return BitmapFactory.decodeByteArray(payload, 0, payload.size)
    }

    private fun findImageMagic(body: ByteArray): Int? {
        val limit = minOf(MAGIC_SEARCH_LIMIT, body.size)
        for (offset in 0 until limit) {
            for (magic in IMAGE_MAGICS) {
                if (startsWith(body, magic, offset)) return offset
            }
        }
        return null
    }

    private fun startsWith(d: ByteArray, magic: ByteArray, offset: Int): Boolean {
        if (offset + magic.size > d.size) return false
        for (i in magic.indices) if (d[offset + i] != magic[i]) return false
        return true
    }

    /**
     * Index of [value] at or after [from], or -1.
     *
     * The stdlib only offers ByteArray.indexOf(element) with no start offset, and both
     * of its callers here are scanning a name string that starts partway into a buffer.
     */
    private fun indexOfByte(d: ByteArray, value: Byte, from: Int): Int {
        for (i in from.coerceAtLeast(0) until d.size) if (d[i] == value) return i
        return -1
    }

    private fun readAt(raf: RandomAccessFile, offset: Long, size: Int): ByteArray {
        if (size <= 0 || offset < 0 || offset >= raf.length()) return ByteArray(0)
        val want = minOf(size.toLong(), raf.length() - offset).toInt()
        if (want <= 0) return ByteArray(0)
        val buf = ByteArray(want)
        raf.seek(offset)
        raf.readFully(buf)
        return buf
    }

    private fun le32(b: ByteArray, o: Int): Int {
        if (o + 4 > b.size) return -1
        return (b[o].toInt() and 0xFF) or
                ((b[o + 1].toInt() and 0xFF) shl 8) or
                ((b[o + 2].toInt() and 0xFF) shl 16) or
                ((b[o + 3].toInt() and 0xFF) shl 24)
    }

    private fun le16(b: ByteArray, o: Int): Int {
        if (o + 2 > b.size) return -1
        return (b[o].toInt() and 0xFF) or ((b[o + 1].toInt() and 0xFF) shl 8)
    }

    private const val ADS_INFO_SIZE = 36
    private const val MAX_RESOURCES = 4096

    /** Upper bound on a candidate icon, to avoid pulling a multi-MB sprite sheet. */
    private const val MAX_ICON_BYTES = 512 * 1024
}