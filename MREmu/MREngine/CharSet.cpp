#include <vmchset.h>
#include <string>
#include <cstring>
#include <locale>
#include <codecvt>

#include <iconv.h>

VMINT vm_ucs2_to_gb2312(VMSTR dst, VMINT size, VMWSTR src) {
	iconv_t ch = iconv_open("GB2312//TRANSLIT", "UCS-2LE");

	char* in_ptr = (char*)src;
	char* out_ptr = dst;

	size_t in_size = CONVERT_CHSET_MAX_LEN; // WARNING
	size_t out_size = size;

	size_t res = iconv(ch, &in_ptr, &in_size, &out_ptr, &out_size);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_gb2312_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	iconv_t ch = iconv_open("UCS-2LE//TRANSLIT", "GB2312");

	char* in_ptr = src;
	char* out_ptr = (char*)dst;

	size_t in_size = strlen(in_ptr); // WARNING
	size_t out_size = size;

	size_t res = iconv(ch, &in_ptr, &in_size, &out_ptr, &out_size);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_ucs2_to_ascii(VMSTR dst, VMINT size, VMWSTR src) {
	iconv_t ch = iconv_open("ASCII//TRANSLIT", "UCS-2LE");

	char* in_ptr = (char*)src;
	char* out_ptr = dst;

	size_t in_size = CONVERT_CHSET_MAX_LEN; // WARNING
	size_t out_size = size;

	size_t res = iconv(ch, &in_ptr, &in_size, &out_ptr, &out_size);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT vm_ascii_to_ucs2(VMWSTR dst, VMINT size, VMSTR src) {
	iconv_t ch = iconv_open("UCS-2LE//TRANSLIT", "ASCII");

	char* in_ptr = src;
	char* out_ptr = (char*)dst;

	size_t in_size = strlen(in_ptr); // WARNING
	size_t out_size = size;

	size_t res = iconv(ch, &in_ptr, &in_size, &out_ptr, &out_size);

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

	if (ch == (iconv_t)-1)
		return VM_CHSET_CONVERT_ERR_PARAM;

	char* in_ptr = src;
	char* out_ptr = dest;

	size_t in_size = CONVERT_CHSET_MAX_LEN; // WARNING
	size_t out_size = dest_size;

	size_t res = iconv(ch, &in_ptr, &in_size, &out_ptr, &out_size);

	iconv_close(ch);
	return VM_CHSET_CONVERT_SUCCESS;
}

VMINT32 vm_get_language_ssc(VMINT8* ssc) {
	if (ssc == 0)
		return -1;
	sprintf(ssc, "*#0044#");
	return 0;
}