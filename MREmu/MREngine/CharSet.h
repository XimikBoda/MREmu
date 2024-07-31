#pragma once
#include <vmsys.h>
#include <string>

std::u8string ucs2_to_utf8(VMWSTR src);

void utf8_to_ucs2(std::u8string src, VMWSTR dest, int size);