#include "Path.h"
#include "MREngine/CharSet.h"
#include <regex>
#include <vmio.h>

using namespace std::string_literals;

namespace Path {

#ifdef ANDROID
	//const fs::path base_path = "/sdcard/Android/data/com.ximikboda.mremu/files/";
	const fs::path base_path = "/sdcard/MREmu/";
#else
	const fs::path base_path = "./";
#endif

	void init()
	{
		std::error_code ec;
#ifdef ANDROID
		fs::create_directories(base_path, ec);
#endif
		fs::create_directories(base_path / fs::path("fs").make_preferred(), ec);
		fs::create_directories(base_path / fs::path("fs/e").make_preferred(), ec);
		fs::create_directories(base_path / fs::path("fs/c").make_preferred(), ec);
		fs::create_directories(base_path / fs::path("fs/d").make_preferred(), ec);
		fs::create_directories(get_screenshots_path(), ec);
	}

	fs::path get_screenshots_path() {
		return (base_path / fs::path("screenshots")).make_preferred();
	}

	bool validate_path(fs::path path) {
		std::regex pr("^[a-ÒA-—]:[\\\\\\/](?:[^\\\\\\/:*?\"<>|\\r\\n]+[\\\\\\/]*)*[^\\\\\\/:*?\"<>|\\r\\n]*$"s); // I hope it`s correct

		return std::regex_match(path.string(), pr);
	}

	fs::path UCS2_to_path(const VMWSTR wstr) {
		auto u8str = ucs2_to_utf8(wstr);
#ifndef WIN32
		std::replace(u8str.begin(), u8str.end(), '\\', '/');
#endif
		return u8str;
	}

	fs::path path_from_emu(fs::path path) { // TODO rewrite this
		fs::path res = base_path / "fs";

		std::string root_n;
		fs::path path_relative = "";
#ifdef WIN32
		root_n = path.root_name().string();
		path_relative = path.relative_path();
#else
		std::wstring str = path.wstring();
		std::replace(str.begin(), str.end(), L'\\', L'/');
		path = str;

		auto it = path.begin();
		if (it == path.end())
			return "";
		root_n = *it;
		it++;
		while (it != path.end()) {
			path_relative /= *it;
			it++;
		}
#endif

		if (root_n.size()) {
			root_n.resize(1);
			if (root_n[0] >= 'A' && root_n[0] <= 'Z')
				root_n[0] -= 'A' - 'a';
			res /= fs::path(root_n);
		}
		else
			res /= "e";

		res /= path_relative;
		res = res.make_preferred(); //works only on windows(
		//std::wcout << "path_from_emu: " << path << " to " << res << '\n';
		return res;
	}

	fs::path path_from_emu(const VMWSTR str) {
		return path_from_emu(UCS2_to_path(str));
	}
}