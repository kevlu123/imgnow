#pragma once
#include <optional>
#include <string>
#include "window.h"
#include "config.h"
#include "colourfmt.h"

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
	void Resized(int width, int height) override;
private:
	void UpdateActiveImage();
	void DrawAlphaBackground() const;
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
	std::vector<ImageEntity>::iterator DeleteImage(ImageEntity* image); // Returns iterator to next image
	void ResetTransform(ImageEntity& image) const;
	void CopyToClipboard() const;
	SDL_Rect GetImageRect() const;
	SDL_Point ScreenToImagePosition(SDL_Point p) const;
	SDL_Point ImageToScreenPosition(SDL_Point p) const;
	bool RotatedPerpendicular() const;
	Config config;
	ColourFormatter colourFormatter;
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
	bool maximized = false;
	mutable std::vector<SDL_Rect> alphaBgCachedSquares;
	mutable SDL_Rect alphaBgCachedRect;
};
