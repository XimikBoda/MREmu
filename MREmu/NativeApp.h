#pragma once
#include "App.h"
#include "AppManagerEvents.h"

class NativeApp : public App
{
public:
	Event::native_app conf; // TODO

	bool preparation() override;
	void start() override;
};
