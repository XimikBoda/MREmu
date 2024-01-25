#include <vmchset.h>
#include <string>
#include <locale>
#include <codecvt>

VMINT vm_ascii_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

	std::string str(src);
	std::wstring wstr = converter.from_bytes(str);
	memcpy(dst, wstr.data(), std::min<size_t>(size, wstr.size()*2 + 2));
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_gb2312_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

	std::string str(src);
	std::wstring wstr = converter.from_bytes(str);
	memcpy(dst, wstr.data(), std::min<size_t>(size, wstr.size()*2 + 2));
	return VM_CHSET_CONVERT_SUCCESS;
}