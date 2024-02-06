#include <vmchset.h>
#include <string>
#include <cstring>
#include <locale>
#include <codecvt>

VMINT vm_ascii_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	std::string str(src);
	std::u16string u16str = converter.from_bytes(str);

	int c_size = u16str.copy((char16_t*)dst, size / 2);
	if (c_size < size * 2)
		dst[c_size] = 0;

	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_gb2312_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

	std::string str(src);
	std::wstring wstr = converter.from_bytes(str);
	memcpy(dst, wstr.data(), std::min<size_t>(size, wstr.size()*2 + 2));
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_ucs2_to_ascii(VMSTR dst, VMINT size, VMWSTR src) {
	std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	std::u16string u16str((char16_t*)src);
	std::string str = converter.to_bytes(u16str);

	int c_size = str.copy((char*)dst, size);
	if (c_size < size)
		dst[c_size] = 0;

	return VM_CHSET_CONVERT_SUCCESS;
}

vm_language_t vm_get_language(void) {
	return ENGLISH; //TEMP
}

VMINT32 vm_get_language_ssc(VMINT8* ssc) {
	if (ssc == 0)
		return -1;
	sprintf(ssc, "*#0044#");
	return 0;
}