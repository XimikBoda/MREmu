#pragma once
#include <string>
#include <vmio.h>

namespace Event {
	/*struct file_selector {
		std::u16string path;
		VMINT(*callback)(VMWCHAR*, VMINT);
		int phandle = -1;
	};*/
}

namespace NativeApps::Applet::TextInput {
	int start(std::u16string def_str, int max_size,
		int input_method, void (*input_text_cb)(VMINT state, VMWSTR text));
}