#include "MreTags.h"
#include "MREngine/CharSet.h"
#include <cstring>
#include <vmcert.h>

using namespace std::string_literals;

bool MreTags::load(std::vector<uint8_t>& file)
{
	raw_tags.clear();

	size_t file_size = file.size();

	if (file_size < 4 * 3)
		return false;

	tags_offset = *(uint32_t*)&file[file_size - 4 * 3];

	uint32_t id, tag_size, pos = tags_offset;

	do {
		if (pos + 8 >= file_size)
			return false;

		id = *(uint32_t*)&file[pos];
		tag_size = *(uint32_t*)&file[pos + 4];
		pos += 8;

		if (pos + tag_size >= file_size)
			return false;

		raw_tags[id].resize(tag_size);
		if (tag_size)
			memcpy(raw_tags[id].data(), &file[pos], tag_size);

		pos += tag_size;
	} while (id);

	return true;
}

bool MreTags::is_ads() {
	if (!is_tag_exist(VM_CE_INFO_FILE_TYPE))
		return 0;

	int t = read_uint32(VM_CE_INFO_FILE_TYPE);
	return (t == 0 || t == 1 || t == 5);
}
bool MreTags::is_simple_ads() {
	int t = read_uint32(VM_CE_INFO_FILE_TYPE);

	return (t == 5);
}

bool MreTags::is_zipped() {
	return read_bool(VM_CE_INFO_RO_RW_ZIP);
}

bool MreTags::is_tags_ucs2() {
	return read_bool(VM_CE_INFO_CHARSET);
}

uint32_t MreTags::get_ram() {
	return read_uint32(VM_CE_INFO_MEM_REQ);
}

std::u8string MreTags::get_dev_name() {
	return read_string(VM_CE_INFO_DEV);
}

bool MreTags::read_bool(int id) {
	return read_uint32(id);
}

bool MreTags::is_tag_exist(int id) {
	auto it = raw_tags.find(id);
	return it != raw_tags.end();
}

uint32_t MreTags::read_uint32(int id) {
	auto it = raw_tags.find(id);
	if (it != raw_tags.end())
		if (it->second.size() == 4)
			return *(uint32_t*)it->second.data();
	return 0;
}

std::u8string MreTags::read_string(int id) {
	auto it = raw_tags.find(id);
	if (it != raw_tags.end()) {
		if (is_tags_ucs2()) {
			auto str16 = std::u16string((char16_t*)it->second.data(), it->second.size());
			return ucs2_to_utf8(str16);
		}
		else
			return std::u8string((char8_t*)it->second.data(), it->second.size());
	}
	return u8"";
}

std::vector<uint8_t> MreTags::get_raw_tag(int id) {
	auto it = raw_tags.find(id);
	if (it != raw_tags.end())
		return it->second;
	return {};
}