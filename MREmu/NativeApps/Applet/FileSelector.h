#pragma once
#include <string>
#include <vmio.h>

namespace Event {
	struct file_selector {
		std::u16string path;
		VMINT(*callback)(VMWCHAR*, VMINT);
		int phandle = -1;
	};
}

namespace NativeApps::Applet::FileSelector {
	vm_selector_error_t start(vm_selector_type_t type, std::u16string str, VMINT(*callback)(VMWCHAR*, VMINT));
}