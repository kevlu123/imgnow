#pragma once
#include <stdint.h> // uint8_t
#include <stddef.h> // size_t
#include <string>
#include <span>
#include <tuple>
#include <memory>
#include <vector>
#include "SDL.h"

struct Image {
	Image() = default;
	Image(const char* path);
	Image(const Image&) = delete;
	Image(Image&&) noexcept = default;
	Image& operator=(const Image&) = delete;
	Image& operator=(Image&&) noexcept = default;
	int GetWidth() const;
	int GetHeight() const;
	float GetAspectRatio() const;
	int GetChannels() const;
	SDL_Colour GetPixel(int x, int y, size_t frame) const;
	const uint8_t* GetPixels() const;
	bool Valid() const;
	const std::string& Error() const;
	
	size_t GetFrameCount() const;
	int GetGifDuration() const;
	int GetGifDelay(size_t frame) const;
private:
	int width = 0;
	int height = 0;
	int channels = 0;
	int duration = 0;
	std::vector<int> delays;
	std::shared_ptr<uint8_t> data; // Unique but uses custom deleter
	std::string error;
};
