#include "Resources.h"
#include "../Bridge.h"
#include "../Cpu.h"
#include "../AppManager.h"
#include <cstring>
#include <vmres.h>
#include <vm4res.h>
#include <vmgraph.h>
#include <ranges>
#include <algorithm>

static std::string to_lower(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return std::tolower(c); });

	return str;
}

void MREngine::Resources::scan()
{
	size_t pos = offset;

	global = false;

	while (1) {
		if (pos > offset + size)
			abort();

		if (!(*file_context)[pos])
			break;

		std::string name = (char*)(file_context->data() + pos);
		pos += name.length() + 1;

		uint32_t res_offset = *(uint32_t*)(file_context->data() + pos);
		pos += 4;

		uint32_t res_size = *(uint32_t*)(file_context->data() + pos);
		pos += 4;

		if (res_offset < offset || global)
			res_offset += offset, global = true;

		if (res_offset < offset || res_offset + res_size > offset + size)
			printf("Warning: resource outside the resource segment (%s)\n", name.c_str());

		if (res_offset + res_size > file_context->size())
			abort();

		res_map[to_lower(name)] = {res_offset, res_size};

		if (name == "mre-2.0")
			scan_mre2_0(res_offset, res_size);
	}
}

void MREngine::Resources::scan_mre2_0(uint32_t offset, uint32_t size)
{
	size_t pos = offset;

	while (1) {
		if (pos > offset + size)
			abort();

		uint32_t id = *(uint32_t*)(file_context->data() + pos);
		pos += 4;

		if (id == 0xFFFFFFFF)
			break;

		uint32_t res_offset = *(uint32_t*)(file_context->data() + pos);
		pos += 4;

		uint32_t res_next_offset = *(uint32_t*)(file_context->data() + pos + 4);

		uint32_t res_size = res_next_offset - res_offset;
		if (res_offset > res_next_offset)
			abort();

		if (res_offset < this->offset || res_offset + res_size > this->offset + this->size)
			abort();

		res2_map[id] = { res_offset, res_size };
	}
}

MREngine::res_el* MREngine::Resources::find_by_name(std::string name)
{
	auto it = res_map.find(name);
	if (it == res_map.end())
		return 0;
	else
		return &it->second;
}

MREngine::res_el* MREngine::Resources::find_by_id(uint32_t id)
{
	auto it = res2_map.find(id);
	if (it == res2_map.end())
		return 0;
	else
		return &it->second;
}

unsigned char* MREngine::Resources::get_file_context()
{
	return file_context->data();
}

extern AppManager* g_appManager;

uint8_t* MREngine::Resources::call_res_provider(int resid, int* len)
{
	auto app = g_appManager->get_current_work_app_id();

	return app->run<true>(res_provider, resid, len);
}


VMUINT8* vm_load_resource(char* res_name, VMINT* res_size) {
	MREngine::Resources& resources = get_current_app_resources();

	printf("vm_load_resource(%s)\n", res_name);

	MREngine::res_el* res = resources.find_by_name(to_lower(res_name));

	if (!res)
		return 0;

	*res_size = res->size;

	void* adr = vm_malloc(res->size);

	if (!adr)
		return 0;

	memcpy(adr, resources.get_file_context() + res->offset, res->size);

	return (VMUINT8*)adr;
}

VMINT32 vm_resource_get_data(VMUINT8* data, VMUINT32 offset, VMUINT32 size) {
	MREngine::Resources& resources = get_current_app_resources();
	if (data == 0)
		return -1;

	if(offset + size >= resources.file_context->size())
		return -1;

	memcpy(data, resources.file_context->data() + offset, size);
	return 0;
	
}

VMUINT vm_get_resource_offset(char* res_name) {
	MREngine::Resources& resources = get_current_app_resources();

	MREngine::res_el* res = resources.find_by_name(to_lower(res_name));

	if (!res)
		return 0;
	
	return res->offset;
}

VMUINT vm_get_resource_offset_from_file(VMWSTR filename, char* res_name) {
	return vm_get_resource_offset(res_name); //todo
}

VMINT vm_get_res_header() {
	return 8;
}

VMINT32 vm_res_init(void) {
	return vm_res_init_with_language(44); //todo
}


VMINT32 vm_res_init_with_language(VMUINT32 language) {
	MREngine::Resources& resources = get_current_app_resources();

	MREngine::res_el* res = resources.find_by_id(language | 0xFFFF0000);

	if (!res)
		return VM_RES_ITEM_NOT_FOUND;

	if (resources.res2_strings)
		vm_res_deinit();

	uint32_t strings_count = *(uint32_t*)(resources.file_context->data() + res->offset);

	uint32_t strings_beg_offset = 4 + 8 * strings_count;
	uint32_t strings_size = res->size - strings_beg_offset;

	resources.res2_strings = vm_malloc(strings_size);
	if (!resources.res2_strings)
		return VM_RES_OUT_OF_MEM;

	memcpy(resources.res2_strings, resources.file_context->data() + res->offset + strings_beg_offset, strings_size);

	size_t pos = res->offset + 4;

	for (int i = 0; i < strings_count; ++i) {
		if (pos > res->offset + res->size)
			abort();

		uint32_t id = *(uint32_t*)(resources.file_context->data() + pos);
		pos += 4;

		if (id == 0xFFFFFFFF)
			break;

		uint32_t res_offset = *(uint32_t*)(resources.file_context->data() + pos);
		pos += 4;

		resources.res2_strings_map[id] = {
			(char*)resources.res2_strings + res_offset - strings_beg_offset
		};
	}

	return VM_RES_SUCCESS;
}

VMINT32 vm_res_deinit(void) {
	MREngine::Resources& resources = get_current_app_resources();

	if (resources.res2_strings) {
		vm_free(resources.res2_strings);
		resources.res2_strings = NULL;

		resources.res2_strings_map.clear();

		return VM_RES_SUCCESS;
	}

	return VM_RES_ITEM_NOT_FOUND;
}

VMUINT8* vm_res_get_string(VMUINT32 string_id) {
	MREngine::Resources& resources = get_current_app_resources();

	auto it = resources.res2_strings_map.find(string_id);
	if (it != resources.res2_strings_map.end())
		return (VMUINT8*)it->second;
	else
		return 0;
}

VMUINT8* vm_res_get_image_and_size(VMUINT32 image_id, VMUINT32* size) {
	MREngine::Resources& resources = get_current_app_resources();

	MREngine::res_el* res = resources.find_by_id(image_id);

	if (!res)
		return 0;

	*size = res->size;

	auto it = resources.allocated_res_map.find(image_id);
	if (it != resources.allocated_res_map.end())
		return (VMUINT8*)&it->second;
	else {
		void* adr = vm_malloc(res->size);

		if (!adr)
			return 0;

		memcpy(adr, resources.get_file_context() + res->offset, res->size);

		resources.allocated_res_map[image_id] = adr;

		return (VMUINT8*)adr;
	}
}

VMUINT8* vm_res_get_image(VMUINT32 image_id) {
	VMUINT32 size = 0;
	return vm_res_get_image_and_size(image_id, &size);
}

VMUINT8* vm_res_get_audio(VMUINT32 audio_id) {
	VMUINT32 size = 0;
	return vm_res_get_image_and_size(audio_id, &size);
}

VMINT32 vm_res_delete(VMUINT32 id) {
	MREngine::Resources& resources = get_current_app_resources();

	auto it = resources.allocated_res_map.find(id);
	if (it == resources.allocated_res_map.end())
		return VM_RES_ITEM_NOT_FOUND;

	resources.allocated_res_map.erase(id);
	return VM_RES_SUCCESS;
}

void vm_reg_res_provider(VMUINT8* (*fp)(VMINT resid, VMINT* len)) {
	MREngine::Resources& resources = get_current_app_resources();

	resources.res_provider = fp;
}