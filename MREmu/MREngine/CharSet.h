#pragma once
#include <vmsys.h>
#include <string>

namespace MREngine {
	namespace CharSet {
		void init();
	}
}

std::u8string ucs2_to_utf8(VMWSTR src); 
std::u8string ucs2_to_utf8(std::u16string src);
std::u8string gb2312_to_utf8(std::string src);

std::u16string utf8_to_utf16(std::u8string src);

void utf8_to_ucs2(std::u8string src, VMWSTR dest, int size);

std::string utf8_to_ascii(std::u8string src);