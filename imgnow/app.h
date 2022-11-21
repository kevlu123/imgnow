#pragma once
#include "window.h"

struct ImageEntity {
	std::future<Image> future;
	Image image;
	SDL_Texture* texture;
	struct {
		float x = 0;
		float y = 0;
		float scale = 1;
		bool flipHorizontal = false;
		bool flipVertical = false;
		int rotation = 0; // In 90 degree anti-clockwise units
	} transform;
};

struct App : Window {
	App(int argc, char** argv);
	~App();
	void Update(float dt) override;
	void Render() const override;
private:
	void CheckImageFinishedLoading();
	std::vector<ImageEntity> images;
	size_t activeImageIndex = 0;
	struct { int x, y; } dragLocation{};
};
