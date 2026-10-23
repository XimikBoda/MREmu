#include "IO.h"
#include "CharSet.h"
#include "../Memory.h"
#include "../Log.h"
#include "../AppContext.h"
#include "../Path.h"
#include <vmio.h>
#include <iostream>
#include <cstring>
#include <regex>
#include <codecvt>
#include <filesystem>

#ifdef DEBUG_WINDOW
#include <imgui.h>
#include <imgui-SFML.h>
#endif

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
	path = Path::path_from_emu(path_f);
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
			spdlog::error("regex faild ({})", find_line);
		}
	}

	if (!fs::exists(path.parent_path()))
		return;

	std::error_code ec;

	di = fs::directory_iterator(path.parent_path(), ec);

	if(ec)
		return;

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
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	io.key_handler = handler;
}

void vm_reg_pen_callback(vm_pen_handler_t handler) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	io.pen_handler = handler;
}

VMFILE vm_file_open(const VMWSTR filename, VMUINT mode, VMUINT binary) {
	if (!filename)
		return -1;

	MREngine::AppIO& io = AppContext::get_executing_app_io();

	fs::path path = Path::path_from_emu(filename);

	std::string fmode;
	std::string fmode2;

	switch (mode) {
	case MODE_READ:
		fmode = "rb";
		break;
	case MODE_WRITE:
		fmode = "r+b";
		fmode2 = "w+b";
		break;
	case MODE_CREATE_ALWAYS_WRITE:
		fmode = "w+b";
		break;
	case MODE_APPEND:
		fmode = "a+b";
		fmode2 = "w+b";
		break;
	default:
		return -1;
	}

	std::error_code ec;
	if (mode != MODE_READ) 
		fs::create_directories(path.parent_path(), ec); // This is right way?

#ifdef _WIN32
	std::wstring wfmode(fmode.begin(), fmode.end());
	FILE* f = _wfopen(path.wstring().c_str(), wfmode.c_str());
	if (!f && fmode2.size()) {
		std::wstring wfmode(fmode2.begin(), fmode2.end());
		f = _wfopen(path.wstring().c_str(), wfmode.c_str());
	}
#else
	FILE* f = fopen(path.string().c_str(), fmode.c_str());
	if (!f && fmode2.size()) {
		f = fopen(path.string().c_str(), fmode2.c_str());
	}
#endif

	if (!f) {
		spdlog::warn("vm_file_open({}, {}) = -2", path.string(), mode);
		return -2;
	}

	return io.files.push(f);
}

void vm_file_close(VMFILE handle) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (io.files.is_active(handle)) {
		FILE* f = io.files[handle];
		if (f)
			fclose(f);
		io.files[handle] = NULL;
		io.files.remove(handle);
	}
}

VMINT vm_file_read(VMFILE handle, void* data, VMUINT length, VMUINT* nread) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.files.is_active(handle) || !io.files[handle] || !data || !nread)
		return -1;

	FILE* f = io.files[handle];

	size_t read_bytes = fread(data, 1, length, f);
	*nread = (VMUINT)read_bytes;

	return *nread;
}

VMINT vm_file_write(VMFILE handle, void* data, VMUINT length, VMUINT* written) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.files.is_active(handle) || !io.files[handle] || !data || !written)
		return -1;

	FILE* f = io.files[handle];

	size_t written_bytes = fwrite(data, 1, length, f);
	*written = (VMUINT)written_bytes;

	if (written_bytes != length && ferror(f)) {
		return -1;
	}

	return *written;
}

VMINT vm_file_commit(VMFILE handle) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.files.is_active(handle) || !io.files[handle])
		return -1;

	if (fflush(io.files[handle]) != 0) {
		return -1;
	}

	return 0;
}

VMINT vm_file_seek(VMFILE handle, VMINT offset, VMINT base) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.files.is_active(handle) || !io.files[handle])
		return -1;

	int origin;

	switch (base) {
	case BASE_BEGIN: origin = SEEK_SET; break;
	case BASE_CURR:  origin = SEEK_CUR; break;
	case BASE_END:   origin = SEEK_END; break;
	default:         return -1;
	}

	if (fseek(io.files[handle], offset, origin) != 0) {
		return -1;
	}

	return 0;
}

VMINT vm_file_tell(VMFILE handle) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.files.is_active(handle) || !io.files[handle])
		return -1;

	long pos = ftell(io.files[handle]);
	if (pos == -1L) {
		return -1;
	}

	return (VMINT)pos;
}

VMINT vm_file_is_eof(VMFILE handle) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.files.is_active(handle) || !io.files[handle])
		return -1;

	return feof(io.files[handle]) ? 1 : 0;
}

VMINT vm_file_getfilesize(VMFILE handle, VMUINT* file_size) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.files.is_active(handle) || !io.files[handle] || !file_size)
		return -1;

	FILE* f = io.files[handle];

	long current_pos = ftell(f);
	if (current_pos == -1L) return -1;

	if (fseek(f, 0, SEEK_END) != 0) return -1;
	long size = ftell(f);

	fseek(f, current_pos, SEEK_SET);

	if (size == -1L) return -1;

	*file_size = (VMUINT)size;
	return 0;
}

VMINT vm_file_delete(const VMWSTR filename) {
	fs::path path = Path::path_from_emu(filename);

	std::error_code ec;

	fs::remove(path);
	
	if (!ec)
		return 0;
	else 
		return -1;
}

VMINT vm_file_rename(const VMWSTR filename, const VMWSTR newname) {
	fs::path old_path = Path::path_from_emu(filename);
	fs::path new_path = Path::path_from_emu(newname);

	if (!fs::exists(old_path))
		return -1;

	std::error_code ec;

	fs::rename(old_path, new_path, ec);

	if (!ec)
		return 0;
	else
		return -1;
}

VMINT vm_file_mkdir(const VMWSTR dirname) {
	fs::path path = Path::path_from_emu(dirname);
	std::error_code ec;

	bool res = fs::create_directories(path, ec);

	if (!ec)
		return 0;
	else
		return -1;
}

VMINT vm_file_set_attributes(const VMWSTR filename, VMBYTE attributes) {
	fs::path path = Path::path_from_emu(filename);
	return 0; //todo
}

VMINT vm_file_get_attributes(const VMWSTR filename) {
	fs::path path = Path::path_from_emu(filename);
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
	info->size = fs::file_size(Path::path_from_emu(el)); // TODO: Add check for folders
}

VMINT vm_find_first(VMWSTR pathname, struct vm_fileinfo_t* info) {
	if (info == 0 || pathname == 0)
		return -1;
	MREngine::find_el find(Path::UCS2_to_path(pathname));

	fs::path el = find.next();

	if (el.empty())
		return -1;

	find_packer(el, info);

	MREngine::AppIO& io = AppContext::get_executing_app_io();

	return io.find.push(find);
}

VMINT vm_find_next(VMINT handle, struct vm_fileinfo_t* info) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

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
	MREngine::AppIO& io = AppContext::get_executing_app_io();

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
		direntry->filesize = fs::file_size(Path::path_from_emu(el));
	//todo
}

VMINT vm_find_first_ext(VMWSTR pathname, vm_fileinfo_ext* direntry) {
	if (direntry == 0 || pathname == 0)
		return -1;

	MREngine::find_el find(Path::UCS2_to_path(pathname));

	fs::path el = find.next();

	if (el.empty())
		return -1;

	find_ext_packer(el, direntry);

	MREngine::AppIO& io = AppContext::get_executing_app_io();

	return io.find_ext.push(find);
}

VMINT vm_find_next_ext(VMINT handle, vm_fileinfo_ext* direntry) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	if (!io.find_ext.is_active(handle))
		return -1;

	auto& f = io.find_ext[handle];
	fs::path el = f.next();

	if (el.empty())
		return -1;

	find_ext_packer(el, direntry);

	return 0;
}

void vm_find_close_ext(VMINT handle) {
	MREngine::AppIO& io = AppContext::get_executing_app_io();

	io.find_ext.remove(handle);
}

VMINT vm_file_get_modify_time(const VMWSTR filename, vm_time_t* modify_time) {
	fs::path path = Path::path_from_emu(filename);

	if (!fs::exists(path))
		return -1;

	auto fileTime = fs::last_write_time(path);

    std::time_t time;
#if defined(_WIN32)
    auto systemTime = std::chrono::utc_clock::to_sys(std::chrono::file_clock::to_utc(fileTime));
    time = std::chrono::system_clock::to_time_t(systemTime);
#else
    auto sysTimeHighRes = std::chrono::file_clock::to_sys(fileTime);
    auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(sysTimeHighRes);
    time = std::chrono::system_clock::to_time_t(systemTime);
#endif

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

VMINT vm_is_support_pen_touch(void) {
	return 1;
}
