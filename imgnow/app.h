#pragma once
#include "window.h"

struct ImageEntity {
	std::future<Image> future;
	Image image;
	SDL_Texture* texture;
};

struct App : Window {
	App(int argc, char** argv);
	~App();
	void Update() override;
	void Render() const override;
private:
	void CheckImageFinishedLoading();
	std::vector<ImageEntity> images;
};
