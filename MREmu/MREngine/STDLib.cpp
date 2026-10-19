#include "../Memory.h"
#include <vmstdlib.h>
#include <cstring>
#include <cstdint>
#include <stdlib.h>

int strtoi(char* s) {
	return atoi(s);
}

#pragma pack(push, 1)
struct vm_dyn_array_t_emu {
	VMINT block_size;
	VMINT init_size;
	VMINT inc_size;
	VMINT count;
	VMINT capacity;
	uint32_t ptr;
};
#pragma pack(pop)

vm_dyn_array_t* vm_create_dyn_array(VMINT init_size, VMINT inc_size, VMINT block_size) {
	if (init_size < 0 || inc_size <= 0 || block_size < 0)
		return 0;

	vm_dyn_array_t_emu* array = (vm_dyn_array_t_emu*)vm_calloc(sizeof(vm_dyn_array_t_emu));
	if (!array)
		return 0;

	void* buf = vm_malloc(init_size * block_size);
	if (!buf && init_size > 0 && block_size > 0) {
		vm_free(array);
		return 0;
	}

	array->block_size = block_size;
	array->init_size = init_size;
	array->inc_size = inc_size;
	array->count = 0;
	array->capacity = init_size;
	array->ptr = ADDRESS_TO_EMU(buf);

	return (vm_dyn_array_t*)array;
}

VMINT vm_dyn_array_add(vm_dyn_array_t* arr, void* data) {
	vm_dyn_array_t_emu* array = (vm_dyn_array_t_emu*)arr;
	if (!array || !data)
		return -1;

	if (array->count >= array->capacity) {
		int new_size = array->capacity + array->inc_size;
		void* new_ptr = vm_realloc(ADDRESS_FROM_EMU(array->ptr), new_size * array->block_size);
		if (!new_ptr)
			return -1;

		array->capacity = new_size;
		array->ptr = ADDRESS_TO_EMU(new_ptr);
	}

	uint8_t* ptr_8 = (uint8_t*)ADDRESS_FROM_EMU(array->ptr);
	if (!ptr_8)
		return -1;

	memcpy(ptr_8 + array->count * array->block_size, data, array->block_size);

	array->count++;
	return 0;
}

VMINT vm_dyn_array_del(vm_dyn_array_t* arr, VMINT idx) {
	vm_dyn_array_t_emu* array = (vm_dyn_array_t_emu*)arr;
	if (!array || !array->ptr || idx < 0 || idx >= array->count)
		return -1;

	uint8_t* ptr_8 = (uint8_t*)ADDRESS_FROM_EMU(array->ptr);
	if (!ptr_8)
		return -1;

	if (array->count - idx - 1 > 0)
		memmove(ptr_8 + idx * array->block_size,
			ptr_8 + (idx + 1) * array->block_size,
			(array->count - idx - 1) * array->block_size);

	array->count--;
	return 0;
}

VMINT vm_dyn_array_del_all(vm_dyn_array_t* arr) {
	vm_dyn_array_t_emu* array = (vm_dyn_array_t_emu*)arr;
	if (!array)
		return -1;

	array->count = 0;
	return 0;
}

void vm_free_dyn_array(vm_dyn_array_t* arr) {
	vm_dyn_array_t_emu* array = (vm_dyn_array_t_emu*)arr;
	if (array) {
		if (array->ptr)
			vm_free(ADDRESS_FROM_EMU(array->ptr));
		vm_free(array);
	}
}

VMINT vm_safe_wstrcpy(VMWSTR dest, VMINT size, VMWSTR src) {
	if (dest == 0 || src == 0)
		return -1;

	int count = 0;
	for (count = 0; (dest[count] = src[count]) && count < size; ++count);
	if (count == size)
		dest[--count] = 0;
	return count;
}

VMINT vm_wstrlen(VMWSTR s) {
	if (s == 0)
		return -1;
	int count = 0;
	for (count = 0; s[count]; ++count);
	return count;
}

VMINT vm_wstrcpy(VMWSTR dst, const VMWSTR src) {
	if (dst == 0 || src == 0)
		return -1;
	int count = 0;
	for (count = 0; dst[count] = src[count]; ++count);
	return count;
}

VMINT vm_wstrcat(VMWSTR dst, const VMWSTR src) {
	if (dst == 0 || src == 0)
		return -1;
	return vm_wstrcpy(dst + vm_wstrlen(dst), src);
}

VMINT vm_wstrncpy(VMWSTR dst, const VMWSTR src, VMINT length) {
	if (dst == 0 || src == 0)
		return -1;
	int count = 0;
	for (count = 0; (count < length) && (dst[count] = src[count]); ++count);
	return count;
}

VMINT vm_wstrcmp(VMWSTR str_1, VMWSTR str_2) {
	if (str_1 == 0 || str_2 == 0)
		return -1;
	for (; *str_1 == *str_2 || !*str_1; str_1++, str_2++);
	if (*str_1 == *str_2)
		return 0;
	else
		return *str_2 - *str_1;
}

void vm_get_filename(VMWSTR path, VMWSTR filename) {
	if (!path || !filename)
		return;
	int len = vm_wstrlen(path);
	int last_sep = -1;
	for (int i = 0; i < len; ++i) {
		if (path[i] == (VMWCHAR)'\\' || path[i] == (VMWCHAR)'/')
			last_sep = i;
	}
	if (last_sep >= 0)
		vm_wstrcpy(filename, path + last_sep + 1);
	else
		vm_wstrcpy(filename, path);
}

VMINT vm_get_path(VMWSTR fullpath, VMWSTR path) {
	if (!fullpath || !path)
		return -1;
	int len = vm_wstrlen(fullpath);
	int last_sep = -1;
	for (int i = 0; i < len; ++i) {
		if (fullpath[i] == (VMWCHAR)'\\' || fullpath[i] == (VMWCHAR)'/')
			last_sep = i;
	}
	if (last_sep >= 0) {
		vm_wstrncpy(path, fullpath, last_sep + 1);
		path[last_sep + 1] = 0;
	} else {
		path[0] = 0;
	}
	return 0;
}