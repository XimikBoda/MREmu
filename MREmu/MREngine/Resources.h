#pragma once
#include <vector>
#include <string>
#include <map>
#include <cstdint>

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

		bool global = false;

		std::map<std::string, res_el> res_map;
		std::map<uint32_t, res_el> res2_map;

		std::map<uint32_t, void*> res2_strings_map;

		void* res2_strings = NULL;

		std::map<uint32_t, void*> allocated_res_map;

		void scan();
		void scan_mre2_0(uint32_t offset, uint32_t size);

		res_el* find_by_name(std::string name);
		res_el* find_by_id(uint32_t id);

		unsigned char* get_file_context();
	};
}

MREngine::Resources& get_current_app_resources();