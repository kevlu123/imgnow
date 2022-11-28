#pragma once
#include <stdint.h>
#include <string>
#include <unordered_map>

struct Config {
	Config();
	void Save() const;
	void Set(const std::string& key, int value);
	int GetOr(const std::string& key, int default_) const;
	bool TryGet(const std::string& key, int& value) const;
private:
	std::unordered_map<std::string, int32_t> ints;
	bool modified = false;
	std::string filename;
};
