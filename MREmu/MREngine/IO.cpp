#include "IO.h"
#include "../Memory.h"
#include "../MreTags.h"
#include <vmio.h>
#include <vmgettag.h>
#include <iostream>
#include <cstring>
#include <regex>
#include <filesystem>
#include <imgui.h>
#include <imgui-SFML.h>

namespace fs = std::filesystem;

void MREngine::IO::init()
{
	fs::create_directory(".\\fs");
	fs::create_directory(".\\fs\\e");
	fs::create_directory(".\\fs\\c");
	fs::create_directory(".\\fs\\d");
}

static void click_buttom(int key, int ev) {
	add_keyboard_event((ev == 0) ? (VM_KEY_EVENT_DOWN) : (VM_KEY_EVENT_UP), key);
}
struct Keys {
	char name[20] = "";
	int code = 0;
};
Keys keys[3 * 7] =
{
	{"Left S",VM_KEY_LEFT_SOFTKEY},
	{"UP",VM_KEY_UP},
	{"Right S",VM_KEY_RIGHT_SOFTKEY},
	{"LEFT",VM_KEY_LEFT},
	{"OK",VM_KEY_OK},
	{"RIGHT",VM_KEY_RIGHT},
	{" ",VM_KEY_FN},
	{"Down",VM_KEY_DOWN},
	{" ",VM_KEY_FN},
	{"1.,",VM_KEY_NUM1},
	{"2abc",VM_KEY_NUM2},
	{"3def",VM_KEY_NUM3},
	{"4ghi",VM_KEY_NUM4},
	{"5jkl",VM_KEY_NUM5},
	{"6mno",VM_KEY_NUM6},
	{"7pqrs",VM_KEY_NUM7},
	{"8tuv",VM_KEY_NUM8},
	{"9wxyz",VM_KEY_NUM9},
	{"*",VM_KEY_POUND},
	{"0",VM_KEY_NUM0},
	{"#",VM_KEY_STAR},
};

void MREngine::IO::imgui_keyboard() {
	ImVec2 v = { 60,20 };
	ImGui::Begin("KeyBoard");
	for (int i = 0; i < 3 * 7; ++i) {
		if (i % 3 != 0)
			ImGui::SameLine();
		if (ImGui::Button(keys[i].name, v))
			click_buttom(keys[i].code, 1);
		if (ImGui::IsItemClicked())
			click_buttom(keys[i].code, 0);
	}
	ImGui::End();
}

fs::path convert_path(fs::path path) { // TODO rewrite this
	fs::path res = ".\\fs\\";
	//if (path.begin() != path.end() && (++path.begin()) != path.end() && path.begin()->string() == "." && (++path.begin())->string() == "fs")
	//	return path;
	auto litter = path.root_name().string();
	if (litter.size()) {
		litter.resize(2);
		litter[1] = '\\';
		res += litter;
	}
	else {
		res += "e\\";
	}
	res += path.relative_path();
	std::cout << "convert_path: " << path << " to " << res << '\n';
	return res;
}

fs::path convert_path(const VMWSTR str) {
	fs::path path = std::u16string((char16_t*)str); //TODO: fix for linux

	return convert_path(path);
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
	path = convert_path(path_f);
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

	while (find_recv && di != end_itr && !std::regex_match(di->path().filename().string(), find_reg))
		di++;
	if (di == end_itr)
		return "";
	return lfolder / di->path().filename();
}

void vm_reg_keyboard_callback(vm_key_handler_t handler) {
	MREngine::AppIO& io = get_current_app_io();

	io.key_handler = FUNC_TO_UINT32(handler);
}

VMFILE vm_file_open(const VMWSTR filename, VMUINT mode, VMUINT binary) {
	MREngine::AppIO& io = get_current_app_io();

	fs::path path = convert_path(filename);

	std::ios_base::openmode fmode = std::ios_base::binary;

	if (fmode | MODE_READ)
		fmode |= std::ios_base::in;

	if (fmode | MODE_WRITE)
		fmode |= std::ios_base::out; // | std::ios_base::_Nocreate;

	if (fmode | MODE_CREATE_ALWAYS_WRITE)
		fmode |= std::ios_base::out;

	if (fmode | MODE_APPEND)
		fmode |= std::ios_base::out | std::ios_base::app;

	std::fstream* f = new std::fstream;

	f->open(path, fmode);

	if (!f->good()) {
		delete f;
		return -1;
	}

	for (int i = 0; i < io.files.size(); ++i)
		if (io.files[i] == 0) {
			io.files[i] = f;
			return i;
		}

	io.files.push_back(f);

	return io.files.size() - 1;
}

void vm_file_close(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return;

	if (io.files[handle]) {
		io.files[handle]->close();
		delete io.files[handle];
		io.files[handle] = 0; //todo

		while (io.files.size() && io.files[io.files.size() - 1] == 0)
			io.files.resize(io.files.size() - 1);
	}
}

VMINT vm_file_read(VMFILE handle, void* data, VMUINT length, VMUINT* nread) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto f = io.files[handle];
	if (!f)
		return -1;

	f->read((char*)data, length);

	*nread = f->gcount();
	return *nread;
}

VMINT vm_file_write(VMFILE handle, void* data, VMUINT length, VMUINT* written) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto f = io.files[handle];
	if (!f)
		return -1;

	f->write((char*)data, length);

	*written = f->gcount();
	return *written;
}

VMINT vm_file_commit(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto f = io.files[handle];
	if (!f)
		return -1;

	f->flush();
	return 0;
}

VMINT vm_file_seek(VMFILE handle, VMINT offset, VMINT base) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto f = io.files[handle];
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

	f->seekg(offset, sdir);

	if(!f->good())
		return -1;

	return 0;
}

VMINT vm_file_tell(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto f = io.files[handle];
	if (!f)
		return -1;

	return f->tellg();
}

VMINT vm_file_is_eof(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto f = io.files[handle];
	if (!f)
		return -1;

	return f->eof();
}

VMINT vm_file_getfilesize(VMFILE handle, VMUINT* file_size) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto f = io.files[handle];
	if (!f)
		return -1;;

	size_t pos = f->tellg();
	f->seekg(0, std::ios_base::end);

	*file_size = f->tellg();

	f->seekg(pos, std::ios_base::beg);

	return 0;
}

VMINT vm_file_delete(const VMWSTR filename) {
	fs::path path = convert_path(filename);

	if (fs::remove(path))
		return 0;
	else
		return -1;
}

VMINT vm_file_mkdir(const VMWSTR dirname) {
	fs::path path = convert_path(dirname);
	bool res = fs::create_directory(path);
	if (res)
		return 0;
	else
		return -1;
}

VMINT vm_file_set_attributes(const VMWSTR filename, VMBYTE attributes) {
	fs::path path = convert_path(filename);
	return 0; //todo
}

VMINT vm_file_get_attributes(const VMWSTR filename) {
	fs::path path = convert_path(filename);
	int res = 0;

	if (std::filesystem::exists(path)) {
		if (std::filesystem::is_directory(path))
			res |= VM_FS_ATTR_DIR;
		if (!(int)(std::filesystem::status(path).permissions() & fs::perms::owner_write))
			res |= VM_FS_ATTR_READ_ONLY;
	}
	return res;
}

VMINT vm_find_first(VMWSTR pathname, struct vm_fileinfo_t* info) {
	if (info == 0 || pathname == 0)
		return -1;
	MREngine::find_el find((char16_t*)pathname);

	fs::path el = find.next();

	if (el.empty())
		return -1;

	info->filename[el.u16string().copy((char16_t*)info->filename, 260)] = 0;

	MREngine::AppIO& io = get_current_app_io();
	io.find.push_back(find);
	return io.find.size() - 1;
}
VMINT vm_find_next(VMINT handle, struct vm_fileinfo_t* info) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.find.size())
		return -1;

	auto& f = io.find[handle];
	fs::path el = f.next();

	if (el.empty())
		return -1;


	info->filename[el.u16string().copy((char16_t*)info->filename, 260)] = 0;

	return 0;
}
void vm_find_close(VMINT handle) {
	//todo
}

VMINT vm_find_first_ext(VMWSTR pathname, vm_fileinfo_ext* direntry) {
	if (direntry == 0 || pathname == 0)
		return -1;

	MREngine::find_el find((char16_t*)pathname);

	fs::path el = find.next();

	if (el.empty())
		return -1;

	direntry->filefullname[el.u16string().copy((char16_t*)direntry->filefullname, 260)]=0;
	el.stem().u16string().copy((char16_t*)direntry->filename, 8);
	el.extension().u16string().copy((char16_t*)direntry->extension, 3);

	MREngine::AppIO& io = get_current_app_io();
	io.find_ext.push_back(find);
	return io.find_ext.size()-1;
	//todo
}

VMINT vm_find_next_ext(VMINT handle, vm_fileinfo_ext* direntry) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.find_ext.size())
		return -1;

	auto &f = io.find_ext[handle];
	fs::path el = f.next();

	if (el.empty())
		return -1;

	direntry->filefullname[el.u16string().copy((char16_t*)direntry->filefullname, 260)] = 0;
	el.stem().u16string().copy((char16_t*)direntry->filename, 8);
	el.extension().u16string().copy((char16_t*)direntry->extension, 3);

	return 0;
}
void vm_find_close_ext(VMINT handle) {
	//todo
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


VMINT vm_get_vm_tag(short* filename, int tag_num, void* buf, int* buf_size) { // TODO
	if (filename[0] == '@') {
		fs::path vpath = (wchar_t*)filename;
		uint32_t adr;
		sscanf(vpath.stem().string().c_str(), "%d", &adr);
		return vm_get_vm_tag_from_rom((VMUINT8*)ADDRESS_FROM_EMU(adr), tag_num, buf, buf_size);
	}

	fs::path path = convert_path(filename);

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