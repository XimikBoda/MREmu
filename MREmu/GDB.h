#pragma once
#include <string_view>

namespace GDB {
	void wait();
	void update();
	void make_answer(std::string_view str);
}
