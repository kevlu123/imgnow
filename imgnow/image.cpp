#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

Image::Image() {
}

Image::Image(const char* path) : path(path) {
	int channels;
	if (uint8_t* data = stbi_load(path, &width, &height, &channels, 4)) {
		this->data = std::span<uint8_t>(data, width * height * 4);
	} else {
		error = stbi_failure_reason();
	}
}

Image::~Image() {
	if (data.data()) {
		stbi_image_free(data.data());
	}
}

Image::Image(Image&& other) noexcept {
	width = other.width;
	height = other.height;
	data = other.data;
	path = std::move(other.path);
	error = std::move(other.error);
	other.data = {};
}

Image& Image::operator=(Image&& other) noexcept {
	width = other.width;
	height = other.height;
	data = other.data;
	path = std::move(other.path);
	error = std::move(other.error);
	other.data = {};
	return *this;
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

std::span<const uint8_t> Image::GetPixels() const {
	return data;
}

bool Image::Valid() const {
	return data.data() != nullptr;
}

const std::string& Image::Path() const {
	return path;
}

const std::string& Image::Error() const {
	return error;
}
