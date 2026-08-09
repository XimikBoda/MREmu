#include <vmstdlib.h>
#include <cstring>
#include <cstdint>
#include <stdlib.h>

int strtoi(char* s) {
	return atoi(s);
}

vm_dyn_array_t* vm_create_dyn_array(VMINT init_size, VMINT inc_size, VMINT block_size) {
	if (init_size < 0 || inc_size <= 0 || block_size < 0)
		return 0;

	vm_dyn_array_t* array = (vm_dyn_array_t*)vm_calloc(sizeof(vm_dyn_array_t));
	if (!array)
		return 0;

	array->ptr = vm_malloc(init_size * block_size);
	if (!array->ptr) {
		vm_free(array);
		return 0;
	}

	array->block_size = block_size;
	array->init_size = init_size;
	array->inc_size = inc_size;
	array->count = 0;
	array->capacity = init_size;

	return array;
}

VMINT vm_dyn_array_add(vm_dyn_array_t* array, void* data) {
	if (!array || !array->ptr || !data)
		return -1;

	if (array->count >= array->capacity) {
		int new_size = array->capacity + array->inc_size;
		void* new_ptr = vm_realloc(array->ptr, new_size);
		if (!new_ptr)
			return -1;

		array->capacity = new_size;
		array->ptr = new_ptr;
	}

	uint8_t* ptr_8 = (uint8_t*)array->ptr;
	memcpy(ptr_8 + array->count * array->block_size, data, array->block_size);

	array->count++;
	return 0;
}

VMINT vm_dyn_array_del(vm_dyn_array_t* array, VMINT idx) {
	if (!array || !array->ptr || idx < 0 || idx >= array->count)
		return -1;

	uint8_t* ptr_8 = (uint8_t*)array->ptr;
	if((array->count - idx - 1) * array->block_size)
		memmove(ptr_8 + (idx + 1) * array->block_size, 
			ptr_8 + idx * array->block_size,
			(array->count - idx - 1) * array->block_size);
	return 0;
}

VMINT vm_dyn_array_del_all(vm_dyn_array_t* array) {
	if (!array || !array->ptr)
		return -1;

	array->count = 0;
}

void vm_free_dyn_array(vm_dyn_array_t* array) {
	if (array) {
		if (array->ptr)
			vm_free(array->ptr);
		vm_free(array);
	}
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