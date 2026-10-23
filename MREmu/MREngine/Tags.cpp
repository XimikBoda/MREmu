#include "Tags.h"
#include "CharSet.h"
#include "../Path.h"
#include "../Memory.h"
#include "../AppContext.h"
#include <cstring>
#include <fstream>
#include <vmcert.h>
#include <vmgettag.h>

using namespace std::string_literals;

bool MREngine::Tags::load(std::vector<uint8_t>& file)
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

bool MREngine::Tags::is_ads() {
	if (!is_tag_exist(VM_CE_INFO_FILE_TYPE))
		return 0;

	int t = read_uint32(VM_CE_INFO_FILE_TYPE);
	return (t == 0 || t == 1 || t == 5);
}

bool MREngine::Tags::is_vre_ads() {
	if (!is_tag_exist(VM_CE_INFO_COMPILER))
		return 0;

	int t = read_uint32(VM_CE_INFO_COMPILER);
	return t == 1;
}

bool MREngine::Tags::is_simple_ads() {
	int t = read_uint32(VM_CE_INFO_FILE_TYPE);

	return (t == 5);
}

bool MREngine::Tags::is_zipped() {
	return read_bool(VM_CE_INFO_RO_RW_ZIP);
}

bool MREngine::Tags::is_tags_ucs2() {
	return read_bool(VM_CE_INFO_CHARSET);
}

uint32_t MREngine::Tags::get_ram() {
	return read_uint32(VM_CE_INFO_MEM_REQ);
}

std::u8string MREngine::Tags::get_dev_name() {
	return read_string(VM_CE_INFO_DEV);
}

std::u8string MREngine::Tags::get_app_name() {
	if(is_tag_exist(VM_CE_INFO_NAME))
		return read_string(VM_CE_INFO_NAME);
	else if(is_tag_exist(VM_CE_INFO_NAME_LIST)) {
		auto strs = read_mstring(VM_CE_INFO_NAME_LIST);
		if (strs.size())
			return strs[0].second;
	}
	return u8"";
}

std::u8string MREngine::Tags::raw_to_u8(void* data, int size) {
	if (is_tags_ucs2()) {
		auto str16 = std::u16string((char16_t*)data, size / 2);
		return ucs2_to_utf8(str16);
	}
	else
		return gb2312_to_utf8(std::string((char*)data, size));
		//return std::u8string((char8_t*)data, size);
}


bool MREngine::Tags::read_bool(int id) {
	return read_uint32(id);
}

bool MREngine::Tags::is_tag_exist(int id) {
	auto it = raw_tags.find(id);
	return it != raw_tags.end();
}

uint32_t MREngine::Tags::read_uint32(int id) {
	auto it = raw_tags.find(id);
	if (it != raw_tags.end())
		if (it->second.size() == 4)
			return *(uint32_t*)it->second.data();
	return 0;
}

std::u8string MREngine::Tags::read_string(int id) {
	auto it = raw_tags.find(id);
	if (it != raw_tags.end())
		return raw_to_u8(it->second.data(), it->second.size());

	return u8"";
}

MREngine::Tags::mstring MREngine::Tags::read_mstring(int id) {
	mstring res;
	size_t offset = 0;
	auto it = raw_tags.find(id);
	if (it != raw_tags.end()) {
		uint8_t* data = it->second.data();
		size_t size = it->second.size();
		while (offset < size) {
			uint32_t id = *(uint32_t*)(data + offset);
			uint32_t str_size = *(uint32_t*)(data + offset + 4);
			res.push_back({ id, raw_to_u8(data + offset + 8, str_size)});
		}
	}

	return res;
}

std::vector<uint8_t> MREngine::Tags::get_raw_tag(int id) {
	auto it = raw_tags.find(id);
	if (it != raw_tags.end())
		return it->second;
	return {};
}


//MRE API

VMINT vm_get_vm_tag(short* filename, int tag_num, void* buf, int* buf_size) { // TODO
	if (filename[0] == '@') {
		fs::path vpath = Path::UCS2_to_path((VMWSTR)filename);
		uint32_t adr;
		sscanf(vpath.stem().string().c_str(), "%d", &adr);
		return vm_get_vm_tag_from_rom((VMUINT8*)ADDRESS_FROM_EMU(adr), tag_num, buf, buf_size);
	}

	fs::path path = Path::path_from_emu(filename);

	std::fstream f(path, std::ios::in | std::ios::binary | std::ios::ate);

	if (!f.good())
		return GET_TAG_FILE_ERROR;

	size_t file_size = f.tellg();

	if (file_size < 4 * 3)
		return GET_TAG_FILE_ERROR;

	f.seekg(file_size - 4 * 3, std::ios::beg);

	uint32_t tags_offset = 0;
	f.read((char*)&tags_offset, 4);

	if (tags_offset >= file_size - 4 * 3)
		return GET_TAG_FILE_ERROR;

	uint32_t id, tag_size, pos = tags_offset;

	do {
		if (pos + 8 >= file_size)
			return GET_TAG_ERROR;

		f.seekg(pos, std::ios::beg);

		f.read((char*)&id, 4);
		f.read((char*)&tag_size, 4);

		pos += 8;

		if (pos + tag_size >= file_size)
			return GET_TAG_ERROR;

		if (id == tag_num)
			break;

		pos += tag_size;
	} while (id);

	if (id != tag_num)
		return GET_TAG_NOT_FOUND;

	if (*buf_size < tag_size)
		return GET_TAG_INSUFFICIENT_BUF;

	f.read((char*)buf, tag_size);

	*buf_size = tag_size;

	return GET_TAG_TRUE;
}

VMINT vm_get_vm_tag_from_rom(VMUINT8* rom, int tag_num, void* buf, int* buf_size) {
	MREngine::Tags* tags = AppContext::get_tags_by_mem_adr(ADDRESS_TO_EMU(rom));
	if (!tags)
		return GET_TAG_ERROR;

	auto tag_data = tags->get_raw_tag(tag_num);

	int tag_size = tag_data.size();

	if (tag_size == 0)
		return GET_TAG_NOT_FOUND;

	if (*buf_size < tag_size)
		return GET_TAG_INSUFFICIENT_BUF;

	memcpy(buf, tag_data.data(), tag_size);

	*buf_size = tag_size;

	return GET_TAG_TRUE;
}