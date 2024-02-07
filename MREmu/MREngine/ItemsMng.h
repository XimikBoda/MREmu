#pragma once
#include <vector>
#include <cstdint>

template<class T>
class ItemsMng {
	struct ItemsMng_el {
		T el;
		bool active;
	};

	std::vector<ItemsMng_el> vec;
public:
	size_t push(const T& el) {
		for (size_t i = 0; i < vec.size(); ++i)
			if (!vec[i].active) {
				vec[i].el = el;
				vec[i].active = false;
				return i;
			}
		vec.push_back({ el, true });
		return vec.size() - 1;
	}
	bool is_active(size_t i) {
		if (i < 0 || i >= vec.size())
			return false;

		return vec[i].active;
	}
	T& operator[](size_t i) {
		if (i < 0 || i >= vec.size())
			abort();
		return vec[i].el;
	}
	void remove(size_t i) {
		if (!is_active(i))
			return;

		vec[i].active = false;
		while (vec.size() && !vec[vec.size() - 1].active)
			vec.resize(vec.size() - 1);
	}
	size_t size() {
		return vec.size();
	}
};