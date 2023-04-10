#pragma once
#include <optional>
#include <string>
#include <vector>
#include <stack>
#include "window.h"
#include "config.h"
#include "colourfmt.h"
#include "net.h"

struct ImageEntity {
	std::string fullPath;
	std::string name;
	std::future<Image> future;
	Image image;
	size_t currentTextureIndex = 0;
	std::vector<SDL_Texture*> textures;
	uint64_t openTime = 0; // Milliseconds since SDL startup
	bool wasReloaded = false;
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
	SDL_Texture* GetTexture() const;
};

struct App : Window {
	App(int argc, char** argv, Config config, std::unique_ptr<MessageServer> msgServer);
	~App();
	void Update() override;
	void Resized(int width, int height) override;
	void Moved(int x, int y) override;
	void FileDropped(const char* path) override;
private:
	void SetWindowTitle(const char* title) const;
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
	void QueueFileLoad(std::string path, size_t index = (size_t)-1);
	void ShowOpenFileDialog();
	float GetScrollDelta() const;
	void Zoom(SDL_Point pivot, float speed);
	std::vector<ImageEntity>::iterator DeleteImage(ImageEntity* image); // Returns iterator to next image
	void ResetTransform(ImageEntity& image) const;
	void CopyToClipboard() const;
	SDL_Rect GetImageRect() const;
	SDL_Point ScreenToImagePosition(SDL_Point p) const;
	SDL_Point ImageToScreenPosition(SDL_Point p) const;
	bool RotatedPerpendicular() const;
	size_t GetCurrentImageIndex() const;
	void ReloadImage(ImageEntity& image);
	Config config;
	std::unique_ptr<MessageServer> msgServer;
	ColourFormatter colourFormatter;
	std::stack<std::string> openFileHistory;
	std::vector<ImageEntity> images;
	size_t activeImageIndex = 0;
	std::optional<size_t> hoverImageIndex = 0;
	std::optional<SDL_Point> dragLocation;
	float sidebarScroll = 0;
	bool sidebarEnabled = true;
	std::optional<size_t> reorderFrom;
	std::optional<size_t> reorderTo;
	float sidebarAnimatedPosition = 1; // Between 0 and 1
	bool gridEnabled = false;
	bool fullscreen = false;
	int activeLoadThreads = 0;
	int maxLoadThreads = 1;
	bool maximized = false;
	int scrollSpeed = 1;
	uint64_t totalPauseTime = 0;
	bool antialiasing;
	std::optional<SDL_Point> restoredPos{};
	std::optional<SDL_Point> restoredSize{};
	std::optional<uint64_t> lastPauseTime;
	mutable std::vector<SDL_Rect> alphaBgCachedSquares;
	mutable SDL_Rect alphaBgCachedRect;
	mutable std::string titleText;
};
