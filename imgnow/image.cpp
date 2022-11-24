#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

Image::Image(const char* path) {	
	if (uint8_t* data = stbi_load(path, &width, &height, &channels, 4)) {
		this->data = std::shared_ptr<uint8_t>(data, stbi_image_free);
	} else {
		error = stbi_failure_reason();
	}
}

int Image::GetWidth() const {
	return width;
}

int Image::GetHeight() const {
	return height;
}

float Image::GetAspectRatio() const {
	if (height == 0) {
		return 1;
	} else {
		return (float)width / height;
	}
}

int Image::GetChannels() const {
	return channels;
}

uint32_t Image::GetPixel(int x, int y) const {
	const uint8_t* pixel = data.get() + (y * width + x) * 4;
	return (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | pixel[3];
}

const uint8_t* Image::GetPixels() const {
	return data.get();
}

bool Image::Valid() const {
	return (bool)data;
}

const std::string& Image::Error() const {
	return error;
}
