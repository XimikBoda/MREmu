#pragma once
#include <vector>
#include <fstream>

namespace MREngine {
	namespace IO {
		void init();
	};

	class AppIO{
	public:
		std::vector<std::fstream> files;
	};
};

MREngine::AppIO& get_current_app_io();