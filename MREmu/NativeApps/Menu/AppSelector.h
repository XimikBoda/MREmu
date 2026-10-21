#pragma once

struct nativeapp_conf {
	void (*entry)() = 0;
};

namespace NativeApps::Menu::AppSelector {
	void entry();
	void rescan();

	const nativeapp_conf Conf = { entry };
}