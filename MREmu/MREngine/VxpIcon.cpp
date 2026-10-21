#include "VxpIcon.h"
#include <algorithm>
#include <cstring>
#include <string>

// Mirrors the Kotlin VxpIconLoader used by the Android library so both front ends
// accept the same packages. See docs/vxp_icons.md.

namespace MREngine {

namespace {

// Stable 11-byte prefix. The bytes after it differ between packages (see the header
// comment), so matching stops here rather than on a full magic string.
constexpr char VRE_PREFIX[] = "VREAPPLOGO";

// Every observed variant puts the image within the first few bytes of the payload; the
// `JPG` variant carries eight bytes of padding ahead of a PNG.
constexpr size_t MAGIC_SEARCH_LIMIT = 64;

constexpr uint32_t ADS_INFO_SIZE = 36;
constexpr size_t MAX_RESOURCES = 4096;
constexpr size_t MAX_ICON_BYTES = 512 * 1024;

bool starts_with(const uint8_t* d, size_t size, size_t off, const char* magic, size_t len) {
	if (off + len > size) return false;
	return memcmp(d + off, magic, len) == 0;
}

// Length of the image signature at [d, d+size), or 0 when there is not one.
size_t image_signature_len(const uint8_t* d, size_t size) {
	if (size >= 4 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F' && d[3] == '8') return 4;
	if (size >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G' &&
		d[4] == 0x0D && d[5] == 0x0A && d[6] == 0x1A && d[7] == 0x0A) return 8;
	if (size >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return 3;
	if (size >= 2 && d[0] == 'B' && d[1] == 'M') return 2;
	return 0;
}

uint32_t read_le32(const uint8_t* d, size_t size, size_t off) {
	if (off + 4 > size) return 0;
	return (uint32_t)d[off] | ((uint32_t)d[off + 1] << 8) |
		((uint32_t)d[off + 2] << 16) | ((uint32_t)d[off + 3] << 24);
}

uint16_t read_le16(const uint8_t* d, size_t size, size_t off) {
	if (off + 2 > size) return 0;
	return (uint16_t)((uint32_t)d[off] | ((uint32_t)d[off + 1] << 8));
}

// Where an image sits inside a resource, bounded to that resource.
//
// The bound matters: a VRE icon blob is followed by unrelated resources in the same
// file, so an unbounded slice hands the decoder the icon *plus* everything after it
// (measured as 1.5 MB of "GIF" for one 1,776-byte icon before this was clamped).
struct ImageSpan {
	long start = -1;
	size_t length = 0;
};

// Finds the image inside [d, d+size), honouring the VRE wrapper when present.
ImageSpan image_span(const uint8_t* d, size_t size) {
	if (size < 4) return {};

	// A bare image resource with no wrapper.
	if (image_signature_len(d, size)) return { 0, size };

	if (!starts_with(d, size, 0, VRE_PREFIX, sizeof(VRE_PREFIX) - 1)) return {};
	if (size <= 23) return {};

	uint32_t declared = read_le32(d, size, 19);
	// The declared payload size bounds the scan, so a stub icon cannot borrow an image
	// from whatever resource happens to follow it. When it is absent or nonsensical,
	// fall back to the rest of this resource.
	size_t body_end = (declared > 0 && (size_t)declared <= size - 23)
		? 23 + (size_t)declared : size;
	const uint8_t* body = d + 23;
	size_t body_size = body_end - 23;

	const size_t limit = std::min(MAGIC_SEARCH_LIMIT, body_size);
	for (size_t off = 0; off < limit; ++off) {
		if (image_signature_len(body + off, body_size - off))
			return { (long)(23 + off), body_size - off };
	}
	return {};
}

std::vector<uint8_t> slice(const uint8_t* d, size_t size, ImageSpan span) {
	if (span.start < 0 || (size_t)span.start >= size) return {};
	size_t len = std::min(span.length, size - (size_t)span.start);
	if (len == 0) return {};
	return std::vector<uint8_t>(d + span.start, d + span.start + len);
}

struct Res {
	std::string name;
	uint32_t offset;
	uint32_t size;
};

// Ranks a resource name by how likely it is to be the app icon. Mirrors the Android
// side: the MRE convention wins outright and anything unrecognised scores 0 so the
// caller's walk stops rather than picking a sprite sheet.
int icon_score(const std::string& name) {
	std::string n;
	n.reserve(name.size());
	for (char c : name) n.push_back((char)tolower((unsigned char)c));

	if (n == "applogo.img") return 100;
	if (n.find("applogo") != std::string::npos) return 90;
	if (n.rfind("_vreapplogo", 0) == 0) return 85;

	auto ends_with = [&](const char* s) {
		size_t l = strlen(s);
		return n.size() >= l && n.compare(n.size() - l, l, s) == 0;
	};
	bool looks_like_image = ends_with(".img") || ends_with(".png") || ends_with(".gif") ||
		ends_with(".jpg") || ends_with(".jpeg") || ends_with(".bmp");
	if (!looks_like_image) return 0;

	if (n == "icon.png" || n == "icon.gif" || n == "icon.jpg" || n == "icon.img") return 60;
	if (n.find("logo") != std::string::npos) return 50;
	if (n.find("icon") != std::string::npos) return 40;
	return 0;
}

bool elf_res_section(const std::vector<uint8_t>& f, uint32_t* out_off, uint32_t* out_size) {
	if (f.size() < 0x34) return false;
	if (!(f[0] == 0x7F && f[1] == 'E' && f[2] == 'L' && f[3] == 'F')) return false;
	if (f[4] != 1 || f[5] != 1) return false;   // 32-bit, little-endian only

	const uint8_t* d = f.data();
	uint32_t shoff = read_le32(d, f.size(), 0x20);
	uint16_t shentsize = read_le16(d, f.size(), 0x2E);
	uint16_t shnum = read_le16(d, f.size(), 0x30);
	uint16_t shstrndx = read_le16(d, f.size(), 0x32);
	if (shentsize < 40 || shnum == 0 || shstrndx >= shnum) return false;

	size_t strEntry = (size_t)shoff + (size_t)shstrndx * shentsize;
	if (strEntry + 24 > f.size()) return false;
	uint32_t stroff = read_le32(d, f.size(), strEntry + 16);
	uint32_t strsize = read_le32(d, f.size(), strEntry + 20);
	if (strsize == 0 || (size_t)stroff + strsize > f.size()) return false;

	for (uint16_t i = 0; i < shnum; ++i) {
		size_t o = (size_t)shoff + (size_t)i * shentsize;
		if (o + 24 > f.size()) continue;
		// sh_name indexes the string table; it is not a file offset.
		uint32_t nameIdx = read_le32(d, f.size(), o);
		if (nameIdx >= strsize) continue;
		const char* base = (const char*)d + stroff + nameIdx;
		size_t maxLen = strsize - nameIdx;
		if (maxLen >= 7 && !strncmp(base, ".vm_res", 7) && (maxLen == 7 || base[7] == '\0')) {
			*out_off = read_le32(d, f.size(), o + 16);
			*out_size = read_le32(d, f.size(), o + 20);
			return true;
		}
	}
	return false;
}

bool ads_res_section(const std::vector<uint8_t>& f, uint32_t tags_offset,
					 uint32_t* out_off, uint32_t* out_size) {
	if (tags_offset < 4) return false;
	const uint8_t* d = f.data();
	if (read_le32(d, f.size(), tags_offset - 4) != ADS_INFO_SIZE) return false;
	if ((size_t)tags_offset - 4 < ADS_INFO_SIZE) return false;
	size_t base = tags_offset - 4 - ADS_INFO_SIZE;
	if (base + ADS_INFO_SIZE > f.size()) return false;
	// Nine u32s; res_offset and res_size are the last two.
	*out_off = read_le32(d, f.size(), base + 28);
	*out_size = read_le32(d, f.size(), base + 32);
	return true;
}

std::vector<Res> read_resource_dir(const std::vector<uint8_t>& f, uint32_t res_off,
								   uint32_t res_size, bool local_offsets) {
	std::vector<Res> out;
	if (res_size == 0 || (size_t)res_off + res_size > f.size()) return out;

	const uint8_t* d = f.data();
	size_t pos = res_off;
	size_t limit = (size_t)res_off + res_size;
	while (pos < limit) {
		if (d[pos] == 0) break;
		size_t end = pos;
		while (end < limit && d[end] != 0) ++end;
		if (end >= limit) break;
		Res r;
		r.name.assign((const char*)d + pos, end - pos);
		pos = end + 1;
		if (pos + 8 > limit) break;
		uint32_t off = read_le32(d, f.size(), pos); pos += 4;
		uint32_t sz = read_le32(d, f.size(), pos); pos += 4;
		if (local_offsets) off += res_off;
		if (off == 0 || sz == 0) continue;
		if ((size_t)off + sz > f.size()) continue;
		r.offset = off;
		r.size = sz;
		out.push_back(r);
		if (out.size() >= MAX_RESOURCES) break;
	}
	return out;
}

// Second chance: look inside .vm_res, ranked by name.
std::vector<uint8_t> icon_from_resources(const std::vector<uint8_t>& f, uint32_t tags_offset) {
	uint32_t res_off = 0, res_size = 0;
	bool local_offsets = false;

	// Container choice keys off the file magic. The tag that looks like it should
	// decide (VM_CE_INFO_RO_RW_ZIP) is set on plain uncompressed ELFs too.
	const bool is_elf = f.size() >= 4 && f[0] == 0x7F && f[1] == 'E' && f[2] == 'L' && f[3] == 'F';
	bool found = is_elf ? elf_res_section(f, &res_off, &res_size)
						: ads_res_section(f, tags_offset, &res_off, &res_size);
	if (!found) return {};

	std::vector<Res> res = read_resource_dir(f, res_off, res_size, local_offsets);
	std::stable_sort(res.begin(), res.end(), [](const Res& a, const Res& b) {
		return icon_score(a.name) > icon_score(b.name);
	});

	for (const Res& r : res) {
		if (r.size > MAX_ICON_BYTES) continue;
		if (icon_score(r.name) == 0) break;
		std::vector<uint8_t> img = slice(f.data() + r.offset, r.size,
										  image_span(f.data() + r.offset, r.size));
		if (!img.empty()) return img;
	}
	return {};
}

} // namespace

std::vector<uint8_t> extract_vxp_icon(const std::vector<uint8_t>& file) {
	if (file.size() < 32) return {};

	// 1. VRE icon blobs, in file order.
	for (size_t i = 0; i + 4 < file.size(); ++i) {
		if (file[i] != 'V' || file[i + 1] != 'R' || file[i + 2] != 'E' || file[i + 3] != 'A')
			continue;
		if (!starts_with(file.data(), file.size(), i, VRE_PREFIX, sizeof(VRE_PREFIX) - 1))
			continue;
		std::vector<uint8_t> img = slice(file.data() + i, file.size() - i,
										  image_span(file.data() + i, file.size() - i));
		if (!img.empty()) return img;
	}

	// 2. Fall back to the resource directory, for packages whose VRE blob is a stub.
	uint32_t tags_offset = read_le32(file.data(), file.size(), file.size() - 12);
	if (tags_offset && (size_t)tags_offset + 12 <= file.size())
		return icon_from_resources(file, tags_offset);

	return {};
}

} // namespace MREngine