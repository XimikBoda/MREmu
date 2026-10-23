#pragma once
#include "App.h"

#include "NativeApps/Menu/AppSelector.h"

class NativeApp : public App
{
public:
	nativeapp_conf conf;

	bool preparation() override;
	void start() override;
};
