#pragma once
#include "window.h"
#include <optional>

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
		float animatedRotation = 0;
	} display;
};

struct App : Window {
	App(int argc, char** argv);
	~App();
	void Update() override;
private:
	void UpdateActiveImage();
	void UpdateSidebar();
	void DrawGrid() const;
	void UpdateStatus() const;
	void CheckImageFinishedLoading();
	bool SidebarVisible() const;
	bool MouseOverSidebar() const;
	bool TryGetVisibleImage(ImageEntity** image);
	bool TryGetVisibleImage(const ImageEntity** image) const;
	void ResetTransform(ImageEntity& image) const;
	SDL_Rect GetImageRect() const;
	std::vector<ImageEntity> images;
	size_t activeImageIndex = 0;
	std::optional<size_t> hoverImageIndex = 0;
	std::optional<SDL_Point> dragLocation;
	float sidebarScroll = 0;
	bool sidebarEnabled = true;
	float sidebarAnimatedPosition = 1; // Between 0 and 1
	bool gridEnabled = false;
	bool fullscreen = false;
};
