#include "IO.h"
#include "CharSet.h"
#include "../Memory.h"
#include "../MreTags.h"
#include <vmio.h>
#include <vmgettag.h>
#include <iostream>
#include <cstring>
#include <regex>
#include <codecvt>
#include <filesystem>
#include <imgui.h>
#include <imgui-SFML.h>

using namespace std::string_literals;

namespace fs = std::filesystem;

void MREngine::IO::init()
{
	fs::create_directory(fs::path("./fs").make_preferred());
	fs::create_directory(fs::path("./fs/e").make_preferred());
	fs::create_directory(fs::path("./fs/c").make_preferred());
	fs::create_directory(fs::path("./fs/d").make_preferred());
}



bool validate_path(fs::path path){
	std::regex pr("^[a-сA-С]:[\\\\\\/](?:[^\\\\\\/:*?\"<>|\\r\\n]+[\\\\\\/]*)*[^\\\\\\/:*?\"<>|\\r\\n]*$"s); // I hope it`s correct

	return std::regex_match(path.string(), pr);
}

fs::path UCS2_to_path(const VMWSTR wstr){
	auto u8str = ucs2_to_utf8(wstr);
#ifndef WIN32
	std::replace(u8str.begin(), u8str.end(), '\\', '/');
#endif
	return u8str;
}

fs::path path_from_emu(fs::path path) { // TODO rewrite this
	fs::path res = "./fs";

	std::string root_n;
	fs::path path_relative = "";
#ifdef WIN32
	root_n = path.root_name().string();
	path_relative = path.relative_path();
#else
	auto it = path.begin();
	if (it == path.end())
		return "";
	root_n = *it;
	it++;
	while (it != path.end()) {
		path_relative /= *it;
		it++;
	}
#endif

	if (root_n.size()) {
		root_n.resize(1);
		if (root_n[0] >= 'A' && root_n[0] <= 'C')
			root_n[0] -= 'A' - 'a';
		res /= fs::path(root_n);
	}
	else
		return "";

	res /= path_relative;
	res = res.make_preferred(); //works only on windows(
	//std::wcout << "path_from_emu: " << path << " to " << res << '\n';
	return res;
}

fs::path path_from_emu(const VMWSTR str) {
	return path_from_emu(UCS2_to_path(str));
}

static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

MREngine::find_el::find_el(fs::path path_f) {
	path = path_from_emu(path_f);
	lfolder = path_f.parent_path();

	find_recv = false;
	std::string find_line = path.filename().string();
	if (find_line.size()) {
		replaceAll(find_line, "*", ".*");
		try {
			find_reg = std::regex(find_line);
			find_recv = true;
		}
		catch (...) {
			printf("regex faild\n");
		}
	}

	if (!fs::exists(path.parent_path()))
		return;

	di = fs::directory_iterator(path.parent_path());

	first = true;
}

fs::path MREngine::find_el::next() {
	fs::directory_iterator end_itr;
	if (first)
		first = false;
	else
		di++;

	while (find_recv && di != end_itr && 
		!std::regex_match(utf8_to_ascii(di->path().filename().u8string()), find_reg))
		di++;
	if (di == end_itr)
		return "";
	return lfolder / di->path().filename();
}

void vm_reg_keyboard_callback(vm_key_handler_t handler) {
	MREngine::AppIO& io = get_current_app_io();

	io.key_handler = handler;
}

VMFILE vm_file_open(const VMWSTR filename, VMUINT mode, VMUINT binary) {
	MREngine::AppIO& io = get_current_app_io();

	fs::path path = path_from_emu(filename);

	std::ios::openmode fmode = std::ios::binary;

	if (mode & MODE_READ)
		fmode |= std::ios::in;

	if (mode & MODE_WRITE)
		fmode |= std::ios::out; // | std::ios::_Nocreate;

	if (mode & MODE_CREATE_ALWAYS_WRITE)
		fmode |= std::ios::out | std::ios::trunc;

	if (mode & MODE_APPEND)
		fmode |= std::ios::out | std::ios::app;

	std::fstream* f = new std::fstream;

	f->open(path, fmode);

	if (!f->good()) {
		delete f;
		return -1;
	}

	return io.files.push(f);
}

void vm_file_close(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (io.files.is_active(handle)) {
		auto& el = io.files[handle];
		el->close();
		delete el;
		el = NULL;
		io.files.remove(handle);
	}
}

VMINT vm_file_read(VMFILE handle, void* data, VMUINT length, VMUINT* nread) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.files.is_active(handle))
		return -1;

	auto& f = io.files[handle];

	if (!f)
		return -1;

	f->read((char*)data, length);

	*nread = f->gcount();

	if (f->eof()) {
		f->clear();
		f->seekg(0, std::ios_base::end);
	}

	return *nread;
}

VMINT vm_file_write(VMFILE handle, void* data, VMUINT length, VMUINT* written) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.files.is_active(handle))
		return -1;

	auto& f = io.files[handle];

	if (!f)
		return -1;

	f->write((char*)data, length);

	*written = length;
	return *written;
}

VMINT vm_file_commit(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.files.is_active(handle))
		return -1;

	auto& f = io.files[handle];

	if (!f)
		return -1;

	f->flush();
	return 0;
}

VMINT vm_file_seek(VMFILE handle, VMINT offset, VMINT base) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.files.is_active(handle))
		return -1;

	auto& f = io.files[handle];

	if (!f)
		return -1;

	std::ios_base::seekdir sdir;

	switch (base) {
	case BASE_BEGIN:
		sdir = std::ios_base::beg;
		break;
	case BASE_CURR:
		sdir = std::ios_base::cur;
		break;
	case BASE_END:
		sdir = std::ios_base::end;
		break;
	default:
		return -1;
		break;
	}

	auto pos = f->tellg();
	f->seekg(offset, sdir);

	if (!f->good()) {
		f->clear();
		f->seekg(pos, std::ios_base::beg);
		return -1;
	}

	return 0;
}

VMINT vm_file_tell(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.files.is_active(handle))
		return -1;

	auto& f = io.files[handle];

	if (!f)
		return -1;

	return f->tellg();
}

VMINT vm_file_is_eof(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.files.is_active(handle))
		return -1;

	auto& f = io.files[handle];

	if (!f)
		return -1;

	return f->eof();
}

VMINT vm_file_getfilesize(VMFILE handle, VMUINT* file_size) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.files.is_active(handle))
		return -1;

	auto& f = io.files[handle];

	if (!f)
		return -1;

	auto pos = f->tellg();
	f->seekg(0, std::ios_base::end);

	*file_size = f->tellg();

	f->seekg(pos, std::ios_base::beg);

	return 0;
}

VMINT vm_file_delete(const VMWSTR filename) {
	fs::path path = path_from_emu(filename);

	if (fs::remove(path))
		return 0;
	else
		return -1;
}

VMINT vm_file_rename(const VMWSTR filename, const VMWSTR newname) {
	fs::path old_path = path_from_emu(filename);
	fs::path new_path = path_from_emu(newname);

	if (!fs::exists(old_path))
		return -1;

	fs::rename(old_path, new_path);
	return 0;
}

VMINT vm_file_mkdir(const VMWSTR dirname) {
	fs::path path = path_from_emu(dirname);
	bool res = fs::create_directory(path);
	if (res)
		return 0;
	else
		return -1;
}

VMINT vm_file_set_attributes(const VMWSTR filename, VMBYTE attributes) {
	fs::path path = path_from_emu(filename);
	return 0; //todo
}

VMINT vm_file_get_attributes(const VMWSTR filename) {
	fs::path path = path_from_emu(filename);
	int res = 0;

	if (std::filesystem::exists(path)) {
		if (std::filesystem::is_directory(path))
			res |= VM_FS_ATTR_DIR;
		if (!(int)(std::filesystem::status(path).permissions() & fs::perms::owner_write))
			res |= VM_FS_ATTR_READ_ONLY;
		return res;
	}
	else
		return -1;
}

void find_packer(fs::path& el, struct vm_fileinfo_t* info) {
	utf8_to_ucs2(el.filename().u8string(), info->filename, MAX_APP_NAME_LEN);
	info->size = fs::file_size(path_from_emu(el)); // TODO: Add check for folders
}

VMINT vm_find_first(VMWSTR pathname, struct vm_fileinfo_t* info) {
	if (info == 0 || pathname == 0)
		return -1;
	MREngine::find_el find(UCS2_to_path(pathname));

	fs::path el = find.next();

	if (el.empty())
		return -1;

	find_packer(el, info);

	MREngine::AppIO& io = get_current_app_io();

	return io.find.push(find);
}

VMINT vm_find_next(VMINT handle, struct vm_fileinfo_t* info) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.find.is_active(handle))
		return -1;

	auto& f = io.find[handle];
	fs::path el = f.next();

	if (el.empty())
		return -1;

	find_packer(el, info);

	return 0;
}
void vm_find_close(VMINT handle) {
	MREngine::AppIO& io = get_current_app_io();

	io.find.remove(handle);
}

void find_ext_packer(fs::path& el, vm_fileinfo_ext* direntry) {
	utf8_to_ucs2(el.filename().u8string(), direntry->filefullname, MAX_APP_NAME_LEN);

	{//8.1 format, weird realization
		VMCHAR filename[9], extension[4];
		utf8_to_ascii(el.stem().u8string()).copy(filename, 9);
		utf8_to_ascii(el.extension().u8string()).copy(extension, 4);
		memcpy(direntry->filename, filename, 8);
		memcpy(direntry->extension, extension, 3);
	}

	VMWCHAR fullpath[MAX_APP_NAME_LEN * 2];
	utf8_to_ucs2(el.u8string(), fullpath, MAX_APP_NAME_LEN * 2);

	direntry->attributes = vm_file_get_attributes(fullpath);
	if (direntry->attributes & VM_FS_ATTR_DIR)
		direntry->filesize = 0;
	else
		direntry->filesize = fs::file_size(path_from_emu(el));
	//todo
}

VMINT vm_find_first_ext(VMWSTR pathname, vm_fileinfo_ext* direntry) {
	if (direntry == 0 || pathname == 0)
		return -1;

	MREngine::find_el find(UCS2_to_path(pathname));

	fs::path el = find.next();

	if (el.empty())
		return -1;

	find_ext_packer(el, direntry);

	MREngine::AppIO& io = get_current_app_io();

	return io.find_ext.push(find);
}

VMINT vm_find_next_ext(VMINT handle, vm_fileinfo_ext* direntry) {
	MREngine::AppIO& io = get_current_app_io();

	if (!io.find_ext.is_active(handle))
		return -1;

	auto &f = io.find_ext[handle];
	fs::path el = f.next();

	if (el.empty())
		return -1;

	find_ext_packer(el, direntry);

	return 0;
}

void vm_find_close_ext(VMINT handle) {
	MREngine::AppIO& io = get_current_app_io();
	
	io.find_ext.remove(handle);
}

VMINT vm_file_get_modify_time(const VMWSTR filename, vm_time_t* modify_time) {
	fs::path path = path_from_emu(filename);

	if(!fs::exists(path))
		return -1;

	auto fileTime = fs::last_write_time(path);

	std::chrono::time_point<std::chrono::system_clock> systemTime;
#ifdef  __GNUC__
	systemTime = std::chrono::file_clock::to_sys(fileTime);
#else // MSVC, CLANG
	systemTime = std::chrono::utc_clock::to_sys(std::chrono::file_clock::to_utc(fileTime));
#endif

	auto time = std::chrono::system_clock::to_time_t(systemTime);

	std::tm* const pTInfo = std::localtime(&time);
	modify_time->year = 1900 + pTInfo->tm_year;
	modify_time->mon = pTInfo->tm_mon;
	modify_time->day = pTInfo->tm_mday;
	modify_time->hour = pTInfo->tm_hour;
	modify_time->min = pTInfo->tm_min;
	modify_time->sec = pTInfo->tm_sec;

	return 0;
}

VMINT vm_get_removable_driver(void) {
	return 'e';
}

VMINT vm_get_system_driver(void) {
	return 'c';
}

VMUINT vm_get_disk_free_space(VMWSTR drv_name) {
	return 256 * 1024 * 1024;
}

VMINT vm_get_disk_info(const VMCHAR* drv_name, vm_fs_disk_info* fs_disk, vm_fs_di_enum e_di) {
	return -1;
}

VMINT vm_is_support_keyborad(void) {
	return 1;
}


VMINT vm_get_vm_tag(short* filename, int tag_num, void* buf, int* buf_size) { // TODO
	if (filename[0] == '@') {
		fs::path vpath = UCS2_to_path((VMWSTR)filename);
		uint32_t adr;
		sscanf(vpath.stem().string().c_str(), "%d", &adr);
		return vm_get_vm_tag_from_rom((VMUINT8*)ADDRESS_FROM_EMU(adr), tag_num, buf, buf_size);
	}

	fs::path path = path_from_emu(filename);

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
	MreTags* tags = get_tags_by_mem_adr(ADDRESS_TO_EMU(rom));
	if (!tags)
		return GET_TAG_ERROR;

	if (tag_num < 0 || tag_num >= tags->raw_tags.size())
		return GET_TAG_NOT_FOUND;

	int tag_size = tags->raw_tags[tag_num].size();

	if (tag_size == 0)
		return GET_TAG_NOT_FOUND;

	if (*buf_size < tag_size)
		return GET_TAG_INSUFFICIENT_BUF;

	memcpy(buf, tags->raw_tags[tag_num].data(), tag_size);

	*buf_size = tag_size;

	return GET_TAG_TRUE;
}