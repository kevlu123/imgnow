#pragma once
#include <stdint.h> // uint8_t
#include <stddef.h> // size_t
#include <vector>

struct Colour {
	uint8_t r, g, b, a;
};

struct Bitmap {
	size_t width = 0;
	size_t height = 0;
	std::vector<Colour> pixels;
};

Bitmap LoadImage(const char* path);
