#include "Resources.h"
#include <cstring>
#include <vmres.h>

void MREngine::Resources::scan()
{
	size_t pos = offset;

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

		if (res_offset < offset)
			res_offset += offset;

		if (res_offset < offset || res_offset + res_size > offset + size)
			abort();

		res_map[name] = { res_offset, res_size };
	}
}

MREngine::res_el* MREngine::Resources::find_py_name(std::string name)
{
	auto it = res_map.find(name);
	if (it == res_map.end())
		return 0;
	else
		return &it->second;
}

unsigned char* MREngine::Resources::get_file_context()
{
	return file_context->data();
}


VMUINT8* vm_load_resource(char* res_name, VMINT* res_size) {
	MREngine::Resources& resources = get_current_app_resources();

	MREngine::res_el* res = resources.find_py_name(res_name);

	if (!res)
		return 0;

	*res_size = res->size;

	void* adr = vm_malloc(res->size);

	if (!adr)
		return 0;

	memcpy(adr, resources.get_file_context() + res->offset, res->size);

	return (VMUINT8*)adr;
}