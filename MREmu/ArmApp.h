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
#include <elfio/elfio.hpp>
#include <elfio/elf_types.hpp>
#include "Bridge.h"

namespace fs = std::filesystem;

class ArmApp : public App
{
public:
	uint32_t entry_point;

	bool is_ads;
	bool is_zipped;

	bool preparation() override;
	void start() override;
	bool load_from_file(fs::path path, bool local) override;//tmp

	bool is_native() { return false; }
};