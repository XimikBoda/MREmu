#include <vmchset.h>
#include <vmstdlib.h>
#include <vmmissing.h>
#include <string>
#include <cstring>
#include <locale>
#include <codecvt>
#include <vector>
#include "CharSet.h"
#include "../Memory.h"

#include "iconv.h"

VMWSTR ucs2_buf = 0;

void MREngine::CharSet::init() {
	ucs2_buf = (VMWSTR)Memory::shared_malloc(256 * 2);
}

static void do_iconv(iconv_t ch, char* in_buf, size_t in_size, char* out_buf, size_t out_size, size_t* written) {
	char* in_ptr = in_buf;
	char* out_ptr = out_buf;
	iconv(ch, (const char**)&in_ptr, &in_size, &out_ptr, &out_size);
	if (written) *written = out_ptr - out_buf;
}

VMINT vm_ucs2_to_gb2312(VMSTR dst, VMINT size, VMWSTR src) {
	iconv_t ch = iconv_open("GB2312//TRANSLIT", "UCS-2LE");
	if (ch == (iconv_t)-1) return VM_CHSET_CONVERT_ERR_PARAM;

	size_t in_size = vm_wstrlen(src) * 2 + 2;
	do_iconv(ch, (char*)src, in_size, dst, size, NULL);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_gb2312_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	iconv_t ch = iconv_open("UCS-2LE//TRANSLIT", "GB2312");
	if (ch == (iconv_t)-1) return VM_CHSET_CONVERT_ERR_PARAM;

	size_t in_size = strlen(src) + 1;
	do_iconv(ch, src, in_size, (char*)dst, size, NULL);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_ucs2_to_ascii(VMSTR dst, VMINT size, VMWSTR src) {
	iconv_t ch = iconv_open("ASCII//TRANSLIT", "UCS-2LE");
	if (ch == (iconv_t)-1) return VM_CHSET_CONVERT_ERR_PARAM;

	size_t in_size = vm_wstrlen(src) * 2 + 2;
	do_iconv(ch, (char*)src, in_size, dst, size, NULL);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_ascii_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	iconv_t ch = iconv_open("UCS-2LE//TRANSLIT", "ASCII");
	if (ch == (iconv_t)-1) return VM_CHSET_CONVERT_ERR_PARAM;

	size_t in_size = strlen(src) + 1;
	do_iconv(ch, src, in_size, (char*)dst, size, NULL);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

vm_language_t vm_get_language(void) {
	return ENGLISH; //TEMP
}

const char* iconv_names[VM_CHSET_TOTAL] =
{
	"", /* BASE */
	"ASCII",		/* ASCII */
	"ISO-8859-6",	/* ARABIC_ISO */
	"WINDOWS-1256", /* ARABIC_WIN */
	"CP1097",		/* PERSIAN_CP1097 */
	"CP1098",		/* PERSIAN_CP1098 */
	"ISO-8859-4",	/* BALTIC_ISO */
	"WINDOWS-1257", /* BALTIC_WIN */
	"ISO-8859-2",	/* CEURO_ISO */
	"WINDOWS-1250", /* CEURO_WIN */
	"ISO-8859-5",	/* CYRILLIC_ISO */
	"WINDOWS-1251", /* CYRILLIC_WIN */
	"ISO-8859-7",	/* GREEK_ISO */
	"WINDOWS-1253", /* GREEK_WIN */
	"ISO-8859-8",	/* HEBREW_ISO */
	"WINDOWS-1255", /* HEBREW_WIN */
	"ISO-8859-4",	/* LATIN_ISO */
	"ISO-8859-10",	/* NORDIC_ISO */
	"ISO-8859-3",	/* SEURO_ISO */
	"ISO-8859-9",	/* TURKISH_ISO */
	"WINDOWS-1254", /* TURKISH_WIN */
	"ISO-8859-1",	/* WESTERN_ISO */
	"ARMSCII-8",	/* ARMENIAN_ISO */  //!
	"WINDOWS-1252",	/* WESTERN_WIN */
	"BIG5",			/* BIG5 */
	"GB2312",		/* GB2312 */
	"BIG5HKSCS",	/* HKSCS */			//!
	"SJIS",			/* SJIS */
	"GB18030",		/* GB18030 */
	"UTF-7",		/* UTF7 */
	"EUCKR",		/* EUCKR */
	"WINDOWS-874",	/* THAI_WIN */
	"WINDOWS-1258", /* VIETNAMESE_WIN */
	"KOI8-R",		/* KOI8_R */
	"TIS-620",		/* TIS_620 */
	"UTF-16LE",		/* UTF16LE */
	"UTF-16BE",		/* UTF16BE */
	"UTF-8",		/* UTF8 */
	"UCS-2LE",		/* UCS2 */
};

VMINT vm_chset_convert(vm_chset_enum src_type, vm_chset_enum dest_type, VMCHAR* src, VMCHAR* dest, VMINT dest_size) {
	if (src_type < 0 || src_type >= VM_CHSET_TOTAL || dest_type < 0 || dest_type >= VM_CHSET_TOTAL)
		return VM_CHSET_CONVERT_ERR_PARAM;

	iconv_t ch = iconv_open(iconv_names[dest_type], iconv_names[src_type]);
	if (ch == (iconv_t)-1) return VM_CHSET_CONVERT_ERR_PARAM;

	size_t in_size = strlen(src) + 1;

	do_iconv(ch, src, in_size, dest, dest_size, NULL);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT32 vm_get_language_ssc(VMINT8* ssc) {
	if (ssc == 0)
		return -1;
	sprintf(ssc, "*#0044#");
	return 0;
}

VMWSTR vm_ucs2_string(VMSTR s) {
	if (s != 0)
		vm_gb2312_to_ucs2(ucs2_buf, 256, s);
	return ucs2_buf;
}

std::u8string ucs2_to_utf8(VMWSTR src) {
	iconv_t ch = iconv_open("UTF-8//IGNORE", "UCS-2LE");
	if (ch == (iconv_t)-1 || !src) return u8"";

	size_t len = vm_wstrlen(src);
	size_t in_size = len * 2;
	size_t buf_size = len * 3;
	std::vector<uint8_t> buf(buf_size, 0);

	size_t written = 0;
	do_iconv(ch, (char*)src, in_size, (char*)buf.data(), buf_size, &written);

	iconv_close(ch);
	return std::u8string((char8_t*)buf.data(), written);
}

std::u8string ucs2_to_utf8(std::u16string src) {
	iconv_t ch = iconv_open("UTF-8//IGNORE", "UCS-2LE");
	if (ch == (iconv_t)-1 || src.empty()) return u8"";

	size_t in_size = src.size() * 2;
	size_t buf_size = src.size() * 3;
	std::vector<char8_t> buf(buf_size, 0);

	size_t written = 0;
	do_iconv(ch, (char*)src.data(), in_size, (char*)buf.data(), buf_size, &written);

	iconv_close(ch);
	return std::u8string((char8_t*)buf.data(), written);
}

void utf8_to_ucs2(std::u8string src, VMWSTR dest, int size) {
	iconv_t ch = iconv_open("UCS-2LE//IGNORE", "UTF-8");
	if (ch == (iconv_t)-1) return;

	size_t in_size = src.size() + 1; 
	do_iconv(ch, (char*)src.data(), in_size, (char*)dest, size * 2, NULL);

	iconv_close(ch);
}

std::u8string gb2312_to_utf8(std::string src) {
	iconv_t ch = iconv_open("UTF-8//IGNORE", "GB2312");
	if (!src.size() || ch == (iconv_t)-1) return u8"";

	size_t in_size = src.size();
	size_t buf_size = in_size * 3;
	std::vector<char8_t> buf(buf_size, 0);

	size_t written = 0;
	do_iconv(ch, src.data(), in_size, (char*)buf.data(), buf_size, &written);

	iconv_close(ch);
	return std::u8string(buf.data(), written);
}

std::u16string utf8_to_utf16(std::u8string src) {
	iconv_t ch = iconv_open("UCS-2LE//IGNORE", "UTF-8");
	if (ch == (iconv_t)-1 || src.empty()) return u"";

	size_t in_size = src.size();
	size_t chars_count = in_size;

	std::vector<char16_t> buf(chars_count, 0);
	size_t buf_size_bytes = chars_count * sizeof(char16_t);

	size_t written_bytes = 0;
	do_iconv(ch, (char*)src.data(), in_size, (char*)buf.data(), buf_size_bytes, &written_bytes);

	iconv_close(ch);
	return std::u16string(buf.data(), written_bytes / sizeof(char16_t));
}

std::string utf8_to_ascii(std::u8string src) {
	iconv_t ch = iconv_open("ASCII//TRANSLIT", "UTF-8");
	if (ch == (iconv_t)-1 || src.empty()) return "";

	size_t in_size = src.size();
	size_t buf_size = in_size;

	std::vector<char> buf(buf_size, 0);

	size_t written = 0;
	do_iconv(ch, (char*)src.data(), in_size, buf.data(), buf_size, &written);

	iconv_close(ch);
	return std::string(buf.data(), written);
}