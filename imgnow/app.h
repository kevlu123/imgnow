#pragma once
#include "window.h"
#include <optional>
#include <string>

struct ImageEntity {
	std::string path;
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
		SDL_Point selectFrom = { -1, -1 };
		SDL_Point selectTo = { -1, -1 };
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
	void UpdateImageLoading();
	bool MouseOverSidebar() const;
	bool TryGetCurrentImage(ImageEntity** image);
	bool TryGetCurrentImage(const ImageEntity** image) const;
	bool TryGetVisibleImage(ImageEntity** image);
	bool TryGetVisibleImage(const ImageEntity** image) const;
	void QueueFileLoad(std::string path);
	void ShowOpenFileDialog();
	void CloseFile(ImageEntity* image);
	void ResetTransform(ImageEntity& image) const;
	SDL_Rect GetImageRect() const;
	SDL_Point ScreenToImagePositionRaw(SDL_Point p) const; // Does not perform rotation/flipping
	SDL_Point ScreenToImagePosition(SDL_Point p) const; // Performs rotation/flipping
	SDL_Point ImageToScreenPosition(SDL_Point p) const;
	std::vector<ImageEntity> images;
	size_t activeImageIndex = 0;
	std::optional<size_t> hoverImageIndex = 0;
	std::optional<SDL_Point> dragLocation;
	float sidebarScroll = 0;
	bool sidebarEnabled = true;
	float sidebarAnimatedPosition = 1; // Between 0 and 1
	bool gridEnabled = false;
	bool fullscreen = false;
	int activeLoadThreads = 0;
	int maxLoadThreads = 1;
};
