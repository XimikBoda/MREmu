#pragma once
#include <vector>
#include <string>
#include <map>

namespace MREngine {
	struct res_el {
		size_t offset;
		size_t size;
	};

	class Resources {
	public:
		std::vector<unsigned char> *file_context;
		uint32_t offset = 0;
		uint32_t size = 0;

		std::map<std::string, res_el> res_map;

		void scan();

		res_el* find_py_name(std::string name);

		unsigned char* get_file_context();
	};
}

MREngine::Resources& get_current_app_resources();