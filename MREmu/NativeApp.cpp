#include "NativeApp.h"
#include "Memory.h"
#include "Bridge.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

using namespace std::string_literals;

bool NativeApp::preparation() {
	app_name = "Native";
	return true;
}

void NativeApp::start()
{
	run(conf.entry);
}
