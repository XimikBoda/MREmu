#pragma once
#include "MreTags.h"
#include "Memory.h"
#include "App.h"
#include "MREngine/System.h"
#include "MREngine/Resources.h"
#include "MREngine/Graphic.h"
#include "MREngine/Timer.h"
#include "MREngine/IO.h"
#include "MREngine/Sock.h"
#include "MREngine/Audio.h"
#include <filesystem>
#include <vector>

#include "NativeApps/Menu/AppSelector.h"

namespace fs = std::filesystem;

class NativeApp : public App
{
public:
	nativeapp_conf conf;

	bool preparation() override;
	void start() override;
};
