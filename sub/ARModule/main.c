#include <stdio.h>
#include <stdarg.h>

#define MYEXPORT __attribute__((visibility("default"))) __attribute__((__used__))

MYEXPORT int vm_vsprintf(char *buf, const char *fmt, va_list args){
    return vsprintf (buf, fmt, args);
}

MYEXPORT int vm_sprintf(char * buf, const char * fmt, va_list args){
    return vsprintf(buf, fmt, args);
}

MYEXPORT int vm_sscanf(const char *str, const char *fmt, va_list args){
    return vsscanf(str, fmt, args);
}

typedef void* (*malloc_t)(size_t size);
typedef void* (*realloc_t)(void* ptr, size_t newsize);
typedef void (*free_t)(void* ptr);

malloc_t malloc_f = 0;
realloc_t realloc_f = 0;
free_t free_f = 0;

void* malloc(size_t size)
{
	return malloc_f(size);
}
void* realloc(void* ptr, size_t newsize) 
{
	return realloc_f(ptr, newsize);
}
void free(void* ptr)
{
	return free_f(ptr);
}

void* __real__malloc_r(struct _reent* unused, size_t size)
{
	(void)unused;
	void* ret = malloc(size);
	return ret;
}
void* __wrap__malloc_r(struct _reent* unused, size_t size)
{
	(void)unused;
	void* ret = malloc(size);
	return ret;
}

void* _realloc_r(struct _reent* unused, void* ptr, size_t newsize)
{
	(void)unused;
	void* ret = realloc(ptr, newsize);
	return ret;
}
void _free_r(struct _reent* unused, void* ptr)
{
	(void)unused;
	free(ptr);
}

void __assert_func(){}

void* ar[3] = {vm_vsprintf, vm_sprintf, vm_sscanf};

extern unsigned int __init_array_start;
extern unsigned int __init_array_end;

typedef void (**__init_array) (void);

int main(malloc_t m, realloc_t r, free_t f){
	unsigned int i;
	__init_array ptr;
    unsigned int init_array_start = 0, count = 0;

    init_array_start = (unsigned int)&__init_array_start,
	count = ((unsigned int)&__init_array_end - (unsigned int)&__init_array_start) / 4;

    ptr = (__init_array)init_array_start;
	for (i = 1; i < count; i++)
	{
		ptr[i]();
	}

    malloc_f = m;
    realloc_f = r;
    free_f = f;

    return (int)ar;
};
 
void _exit(int code){ 
    return;
}