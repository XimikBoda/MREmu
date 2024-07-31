#include <vmstdlib.h>

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