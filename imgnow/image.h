#pragma once
#include <stdint.h> // uint8_t
#include <stddef.h> // size_t
#include <string>
#include <span>

struct Image {
	~Image();
	Image(const char* path);
	Image(const Image&) = delete;
	Image(Image&&) noexcept;
	Image& operator=(const Image&) = delete;
	Image& operator=(Image&&) noexcept;
	int GetWidth() const;
	int GetHeight() const;
	std::span<const uint8_t> GetPixels() const;
	const std::string& Path() const;
	bool Valid() const;
	const std::string& Error() const;
private:
	int width = 0;
	int height = 0;
	std::span<uint8_t> data;
	std::string path;
	std::string error;
};
