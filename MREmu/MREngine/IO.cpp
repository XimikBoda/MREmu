#include "IO.h"
#include <vmio.h>
#include <vmgettag.h>
#include <iostream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

void MREngine::IO::init()
{
	fs::create_directory(".\\fs");
	fs::create_directory(".\\fs\\e");
	fs::create_directory(".\\fs\\c");
	fs::create_directory(".\\fs\\d");
}

fs::path convert_path(const VMWSTR str) { // TODO rewrite this
	fs::path path = (wchar_t*)str, res = ".\\fs\\";
	//if(string(res.string())== string(path.string(),res.string().length()))
	if (path.begin() != path.end() && (++path.begin()) != path.end() && path.begin()->string() == "." && (++path.begin())->string() == "fs")
		return path;
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

VMFILE vm_file_open(const VMWSTR filename, VMUINT mode, VMUINT binary) {
	MREngine::AppIO& io = get_current_app_io();

	fs::path path = convert_path(filename);

	std::ios_base::openmode fmode = std::ios_base::binary;

	if (fmode | MODE_READ)
		fmode |= std::ios_base::in;

	if (fmode | MODE_WRITE)
		fmode |= std::ios_base::out;  // TODO: _Nocreate is not available on Linux, use an alternative
		// fmode |= std::ios_base::out | std::ios_base::_Nocreate;

	if (fmode | MODE_CREATE_ALWAYS_WRITE)
		fmode |= std::ios_base::out;

	if (fmode | MODE_APPEND)
		fmode |= std::ios_base::out | std::ios_base::app;

	io.files.resize(io.files.size() + 1);

	std::fstream& f = io.files[io.files.size() - 1];

	f.open(path, fmode);

	if (!f.good()) {
		io.files.resize(io.files.size() - 1);
		return -1;
	}

	//io.files.push_back(f);

	return io.files.size() - 1;
}

void vm_file_close(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return;

	io.files[handle].close();
}

VMINT vm_file_read(VMFILE handle, void* data, VMUINT length, VMUINT* nread) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto& f = io.files[handle];

	if (!f.good())
		return -1;

	f.read((char*)data, length);

	*nread = f.gcount();
	return *nread;
}

VMINT vm_file_write(VMFILE handle, void* data, VMUINT length, VMUINT* written) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto& f = io.files[handle];

	if (!f.good())
		return -1;

	f.write((char*)data, length);

	*written = f.gcount();
	return *written;
}

VMINT vm_file_commit(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto& f = io.files[handle];

	if (!f.good())
		return -1;

	f.flush();
	return 0;
}

VMINT vm_file_seek(VMFILE handle, VMINT offset, VMINT base) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto& f = io.files[handle];

	if (!f.good())
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

	f.seekg((std::streamoff)offset, sdir);

	return 0;
}

VMINT vm_file_tell(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto& f = io.files[handle];

	if (!f.good())
		return -1;

	return f.tellg();
}

VMINT vm_file_is_eof(VMFILE handle) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto& f = io.files[handle];

	if (!f.good())
		return -1;

	return f.eof();
}

VMINT vm_file_getfilesize(VMFILE handle, VMUINT* file_size) {
	MREngine::AppIO& io = get_current_app_io();

	if (handle < 0 || handle >= io.files.size())
		return -1;

	auto& f = io.files[handle];

	if (!f.good())
		return -1;

	size_t pos = f.tellg();
	f.seekg(0, std::ios_base::end);

	*file_size = f.tellg();

	f.seekg(pos, std::ios_base::beg);

	return 0;
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