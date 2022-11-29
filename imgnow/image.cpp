#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

Image::Image(const char* path) {
	// Try to read as gif
	bool isGif = false;
	if (FILE* f = stbi__fopen(path, "rb")) {
		stbi__context s{};
		stbi__start_file(&s, f);
		if (stbi__gif_test(&s)) {
			isGif = true;
			int* delays = nullptr;
			int frameCount = 0;
			uint8_t* data = (uint8_t*)stbi__load_gif_main(
				&s,
				&delays,
				&width,
				&height,
				&frameCount,
				&channels,
				4);
			if (data) {
				this->data = std::shared_ptr<uint8_t>(data, stbi_image_free);
				this->delays.insert(this->delays.end(), delays, delays + frameCount);
				free(delays);
				if (width <= 0 || height <= 0) {
					error = "non-positive dimensions";
					this->data.reset();
				}
			}
		}
		fclose(f);
	}

	// Other formats
	if (!isGif) {
		if (uint8_t* data = stbi_load(path, &width, &height, &channels, 4)) {
			this->data = std::shared_ptr<uint8_t>(data, stbi_image_free);
			this->delays.push_back(1);
			if (width <= 0 || height <= 0) {
				error = "non-positive dimensions";
				this->data.reset();
			}
		} else {
			error = stbi_failure_reason();
		}
	}

	duration = 0;
	for (int dur : delays) {
		duration += dur;
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
		return 16.0f / 9.0f;
	} else {
		return (float)width / height;
	}
}

int Image::GetChannels() const {
	return channels;
}

size_t Image::GetFrameCount() const {
	return delays.size();
}

int Image::GetGifDuration() const {
	return duration;
}

int Image::GetGifDelay(size_t frame) const {
	return delays[frame];
}

SDL_Colour Image::GetPixel(int x, int y, size_t frame) const {
	const uint8_t* pixel = data.get() + ((frame * width * height) + (y * width + x)) * 4;
	return { pixel[0], pixel[1], pixel[2], pixel[3] };
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
