#pragma once
#include <string>
#include <functional>
#include <optional>
#include <memory>

namespace GUI {
	class Base;

	struct SoftKey {
		std::u16string text;
		std::function<void(std::shared_ptr<Base> &ui_menu)> func;
	};
	
	struct SoftKeySet {
		enum Keys{
			Left, Ok, Right, Count
		};

		std::optional<SoftKey> keys[3];
	};
}