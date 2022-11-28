#include "config.h"
#include <fstream>
#include <charconv>
#include <filesystem>

namespace fs = std::filesystem;

static const char* CONFIG_FILE = "imgnow.ini";

#ifdef _WIN32
#include <Windows.h>
static fs::path GetExecutablePath() {
	wchar_t buffer[MAX_PATH];
	if (GetModuleFileNameW(nullptr, buffer, MAX_PATH) == 0) {
		// In case of error, just return the current directory
		return fs::current_path();
	}
	return fs::path(buffer);
}
#else
static fs::path GetExecutablePath() {
	return fs::canonical("/proc/self/exe");
}
#endif

Config::Config() {
	try {
		filename = (GetExecutablePath().parent_path() / CONFIG_FILE).string();
	} catch (fs::filesystem_error&) {
		filename = CONFIG_FILE;
	}
	
	std::ifstream f(filename);
	if (!f.is_open())
		return;

	std::string line;
	while (std::getline(f, line)) {
		size_t splitIndex = line.find('=');
		if (splitIndex == std::string::npos)
			continue;
		std::string key = line.substr(0, splitIndex);
		std::string_view value(line.begin() + splitIndex + 1, line.end());
		
		int v;
		auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
		if (ec == std::errc()) {
			ints.insert({ key, v });
		}
	}
}

void Config::Save() const {
	if (!modified)
		return;

	std::ofstream f(filename);
	if (!f.is_open())
		return;

	std::string s;
	for (auto& [key, value] : ints) {
		s += key;
		s += '=';
		s += std::to_string(value);
		s += '\n';
	}
	f.write(s.data(), s.size());
}

void Config::Set(const std::string& key, int value) {
	ints[key] = value;
	modified = true;
}

int Config::GetOr(const std::string& key, int default_) const {
	auto it = ints.find(key);
	if (it == ints.end())
		return default_;
	return it->second;
}

bool Config::TryGet(const std::string& key, int& value) const {
	auto it = ints.find(key);
	if (it == ints.end())
		return false;
	value = it->second;
	return true;
}
