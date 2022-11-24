/*
 * TODO:
 * Cap scroll speed.
 * Fix scrolling randomly inverting.
 * Scroll sidebar by dragging.
 * Rectangular selection.
 * Copy to clipboard.
 * React to window resize.
 * Configuration file.
 * Add app icon.
 * Reorder files.
 * Add linux build script.
 * Scroll bars.
*/

#include "app.h"
#include "tinyfiledialogs.h"
#include <tuple>
#include <type_traits>
#include <ranges>
#include <filesystem>
	
constexpr int SIDEBAR_WIDTH = 100;
constexpr int SIDEBAR_BORDER = SIDEBAR_WIDTH / 10;
static const char* HELP_TEXT = R"(
imgnow
Copyright (c) 2022 Kevin Lu

==============================
 Shortcut		Action

 Ctrl+O		Open File
 Ctrl+W		Close File
 F1		Help
 0-9		Switch to Image
 Q		Rotate Anti-clockwise
 W		Rotate 180 Degrees
 E/R		Rotate Clockwise
 Z		Reset Zoom
 S		Toggle Sidebar
 F		Flip Horizontal
 V		Flip Vertical
==============================
)";

// Since std::future blocks when destroyed, we need to keep it
// alive until the image is loaded. We do this by moving the future into
// a heap allocated future which is periodically checked for completion.
// Being heap allocated means that the destructor does not have to
// be called when the program exits. This leak allows for quick termination.
static std::vector<std::future<Image>*> discardedFutures;

App::App(int argc, char** argv) : Window(1280, 720) {
	maxLoadThreads = std::max(1, (int)std::thread::hardware_concurrency() - 1);
	for (int i = 1; i < argc; i++) {
		QueueFileLoad(argv[i]);
	}

	sidebarEnabled = argc > 2;
}

App::~App() {
	SDL_HideWindow(GetWindow());
	for (auto& image : images) {
		if (image.texture) {
			SDL_DestroyTexture(image.texture);
		}
	}
}

void App::Update() {
	UpdateImageLoading();

	if (GetCtrlKeyDown()) {
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_O)) {
			ShowOpenFileDialog();
		} else if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_W)) {
			ImageEntity* image = nullptr;
			if (TryGetCurrentImage(&image)) {
				CloseFile(image);
			}
		}
	} else {
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_G)) {
			gridEnabled = !gridEnabled;
		}

		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F1)) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Help", HELP_TEXT, GetWindow());
		}

		for (size_t i = 0; i < 10; i++) {
			if (GetKeyPressed((SDL_Scancode)(SDL_Scancode::SDL_SCANCODE_1 + i)) && i < images.size()) {
				activeImageIndex = i;
			}
		}

		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F11)) {
			fullscreen = !fullscreen;
			SDL_SetWindowFullscreen(GetWindow(), fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		}
	}
	
	SDL_SetRenderDrawColor(GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(GetRenderer());

	UpdateActiveImage();
	
	if (gridEnabled) {
		DrawGrid();
	}
	
	UpdateSidebar();

	UpdateStatus();

	SDL_RenderPresent(GetRenderer());
}

void App::UpdateStatus() const {
	const ImageEntity* image = nullptr;
	if (!TryGetVisibleImage(&image)) {
		SDL_SetWindowTitle(GetWindow(), "imgnow");
		return;
	}

	auto [mx, my] = GetMousePosition();
	SDL_Rect rect = GetImageRect();
	
	// Mouse position
	SDL_Point offset{};
	offset.x = (int)std::floor((mx - rect.x) / image->display.scale);
	offset.y = (int)std::floor((my - rect.y) / image->display.scale);

	SDL_Point flippedOffset = offset;
	if (image->display.flipHorizontal) {
		int w = image->display.rotation % 2 == 0 ? image->image.GetWidth() : image->image.GetHeight();
		flippedOffset.x = w - offset.x - 1;
	}
	if (image->display.flipVertical) {
		int h = image->display.rotation % 2 == 1 ? image->image.GetWidth() : image->image.GetHeight();
		flippedOffset.y = h - offset.y - 1;
	}
	offset = flippedOffset;
	
	SDL_Point rotatedOffset{};
	switch (image->display.rotation) {
	case 0:
		rotatedOffset = offset;
		break;
	case 1:
		rotatedOffset.x = offset.y;
		rotatedOffset.y = image->image.GetHeight() - offset.x - 1;
		break;
	case 2:
		rotatedOffset.x = image->image.GetWidth() - offset.x - 1;
		rotatedOffset.y = image->image.GetHeight() - offset.y - 1;
		break;
	case 3:
		rotatedOffset.x = image->image.GetWidth() - offset.y - 1;
		rotatedOffset.y = offset.x;
		break;
	default: std::abort();
	}
	offset = rotatedOffset;
	
	// Pixel colour
	SDL_Rect bounds = { 0, 0, image->image.GetWidth(), image->image.GetHeight() };
	uint32_t colour = SDL_PointInRect(&offset, &bounds) ? image->image.GetPixel(offset.x, offset.y) : 0x00000000;
	char hexColour[9]{};
	for (int i = 0; i < 8; i++) {
		hexColour[i] = "0123456789ABCDEF"[colour >> (28 - i * 4) & 0xF];
	}

	static const std::string SEP = "  |  ";
	std::string text = "imgnow" + SEP
		+ image->path + SEP
		+ "Dim: " + std::to_string(image->image.GetWidth()) + "x" + std::to_string(image->image.GetHeight()) + SEP
		+ "XY: (" + std::to_string(rotatedOffset.x) + ", " + std::to_string(offset.y) + ")" + SEP
		+ "RGBA: " + hexColour + SEP
		+ "Channels: " + std::to_string(image->image.GetChannels()) + SEP
		+ "Zoom: " + std::to_string((int)(image->display.scale * 100)) + "%";
	SDL_SetWindowTitle(GetWindow(), text.c_str());
}

void App::UpdateActiveImage() {
	ImageEntity* image = nullptr;
	if (!TryGetVisibleImage(&image))
		return;

	auto& display = image->display;

	auto [cw, ch] = GetClientSize();
	auto [mx, my] = GetMousePosition();
	auto [_, sy] = GetScrollDelta();
	sy *= GetDeltaTime() * 5;

	// Zoom
	if (sy && !MouseOverSidebar()) {
		float& oldScale = display.scale;
		float newScale = oldScale + sy * oldScale;
		display.x = (display.x - mx) / oldScale * newScale + mx;
		display.y = (display.y - my) / oldScale * newScale + my;
		oldScale = newScale;
	}

	// Begin drag
	else if ((GetMousePressed(SDL_BUTTON_LEFT) || GetMousePressed(SDL_BUTTON_MIDDLE)) && !MouseOverSidebar()) {
		dragLocation = { mx, my };
	}

	// Continue drag
	else if (GetMouseDown(SDL_BUTTON_LEFT) || GetMouseDown(SDL_BUTTON_MIDDLE)) {
		if (dragLocation) {
			display.x += mx - dragLocation.value().x;
			display.y += my - dragLocation.value().y;
			dragLocation.value().x = mx;
			dragLocation.value().y = my;
		}
	}

	// End drag
	else {
		dragLocation = std::nullopt;
	}

	if (!GetCtrlKeyDown()) {
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_Z)) {
			ResetTransform(*image);
		}

		// Flipping:
		// Since flipping is applied before rotation for SDL_RenderCopyEx,
		// we need to take into account the current rotation to toggle the correct flag.
		bool flipH = GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F) && display.rotation % 2 == 0
			|| GetKeyPressed(SDL_Scancode::SDL_SCANCODE_V) && display.rotation % 2 == 1;
		bool flipV = GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F) && display.rotation % 2 == 1
			|| GetKeyPressed(SDL_Scancode::SDL_SCANCODE_V) && display.rotation % 2 == 0;
		if (flipH) {
			display.flipHorizontal = !display.flipHorizontal;
		}
		if (flipV) {
			display.flipVertical = !display.flipVertical;
		}

		// Rotate anti-clockwise
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_Q)) {
			display.rotation--;
			if (display.rotation < 0) {
				display.rotation = 3;
			}
		}

		// Rotate clockwise
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_E) || GetKeyPressed(SDL_Scancode::SDL_SCANCODE_R)) {
			display.rotation = (display.rotation + 1) % 4;
		}

		// Rotate 180 degrees
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_W)) {
			display.rotation = (display.rotation + 2) % 4;
		}
	}

	// Animate rotation smoothly
	float dist = std::abs(display.animatedRotation - display.rotation);
	if (dist < 0.001f) {
		// Close enough, set to a nice clean integer.
		display.animatedRotation = (float)display.rotation;
	} else {
		// Interpolate angle with wrapping
		if (std::abs(display.animatedRotation - 4 - display.rotation) < dist) {
			display.animatedRotation -= 4;
		} else if (std::abs(display.animatedRotation + 4 - display.rotation) < dist) {
			display.animatedRotation += 4;
		}
		display.animatedRotation = std::lerp(display.animatedRotation, (float)display.rotation, 0.2f);
	}

	// Draw image
	SDL_Rect dst = {
		(int)display.x,
		(int)display.y,
		(int)(display.scale * image->image.GetWidth()),
		(int)(display.scale * image->image.GetHeight()),
	};
	
	std::underlying_type_t<SDL_RendererFlip> flip = SDL_RendererFlip::SDL_FLIP_NONE;
	if (display.flipHorizontal)
		flip |= SDL_RendererFlip::SDL_FLIP_HORIZONTAL;
	if (display.flipVertical)
		flip |= SDL_RendererFlip::SDL_FLIP_VERTICAL;

	SDL_RenderCopyEx(
		GetRenderer(),
		image->texture,
		nullptr,
		&dst,
		90 * display.animatedRotation,
		nullptr,
		(SDL_RendererFlip)flip);
}

void App::UpdateSidebar() {
	auto [cw, ch] = GetClientSize();
	auto [_, sy] = GetScrollDelta();

	// Toggle sidebar
	if (!GetCtrlKeyDown() && GetKeyPressed(SDL_Scancode::SDL_SCANCODE_S)) {
		sidebarEnabled = !sidebarEnabled;
	}

	// Animate sidebar
	float animationTargetValue = (float)sidebarEnabled;
	if (std::abs(animationTargetValue - sidebarAnimatedPosition) < 0.001f) {
		sidebarAnimatedPosition = animationTargetValue;
	} else {
		sidebarAnimatedPosition = std::lerp(sidebarAnimatedPosition, animationTargetValue, 0.2f);
	}

	// Draw background
	SDL_Rect sbRc = {
		cw - (int)(SIDEBAR_WIDTH * sidebarAnimatedPosition),
		0,
		SIDEBAR_WIDTH,
		ch
	};
	SDL_SetRenderDrawColor(GetRenderer(), 30, 30, 30, 200);
	SDL_RenderFillRect(GetRenderer(), &sbRc);

	// Mini icons
	hoverImageIndex = std::nullopt;
	float y = 0;
	for (size_t i = 0; i < images.size(); i++) {
		const auto& image = images[i];

		float screenY = y - sidebarScroll;
		SDL_Rect rc{};
		rc.w = SIDEBAR_WIDTH - 2 * SIDEBAR_BORDER;
		rc.x = sbRc.x + SIDEBAR_BORDER;
		rc.y = (int)screenY + SIDEBAR_BORDER;
		rc.h = (int)(rc.w / image.image.GetAspectRatio());
		if (image.texture) {
			SDL_RenderCopy(GetRenderer(), image.texture, nullptr, &rc);
		} else {
			// Texture hasn't loaded yet so fill with placeholder
			SDL_SetRenderDrawColor(GetRenderer(), 0, 0, 0, 255);
			SDL_RenderFillRect(GetRenderer(), &rc);
		}

		// Highlight if cursor is over icon
		if (MouseOverSidebar()) {
			SDL_Point mp{};
			std::tie(mp.x, mp.y) = GetMousePosition();

			SDL_Rect hitbox = {
				sbRc.x,
				(int)screenY + SIDEBAR_BORDER / 2,
				sbRc.w,
				rc.h + SIDEBAR_BORDER,
			};
			if (SDL_PointInRect(&mp, &hitbox)) {
				SDL_SetRenderDrawColor(GetRenderer(), 150, 150, 150, 255);
				SDL_RenderDrawRect(GetRenderer(), &rc);
				hoverImageIndex = i;
				if (GetMousePressed(SDL_BUTTON_LEFT)) {
					// Selected a different image
					activeImageIndex = i;
				}
			}
		}

		// Highlight if image is active
		if (activeImageIndex == i) {
			SDL_SetRenderDrawColor(GetRenderer(), 255, 255, 255, 255);
			SDL_RenderDrawRect(GetRenderer(), &rc);
		}

		// Don't increment y on the last iteration because
		// this value of y is used as a bound for scrolling.
		if (i < images.size() - 1) {
			y += SIDEBAR_BORDER + rc.h;
		}
	}

	// Scroll sidebar
	if (MouseOverSidebar()) {
		sidebarScroll -= sy * GetDeltaTime() * 800;
		if (sidebarScroll > y) {
			sidebarScroll = y;
		} else if (sidebarScroll < 0) {
			sidebarScroll = 0;
		}
	}
}

void App::UpdateImageLoading() {
	// Check if any discarded futures have finished loading
	for (auto it = discardedFutures.begin(); it != discardedFutures.end(); ++it) {
		if ((*it)->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			activeLoadThreads--;
			delete *it;
			it = discardedFutures.erase(it);
			if (it == discardedFutures.end())
				break;
		}
	}

	// Check if any futures have finished loading
	for (size_t i = 0; i < images.size(); i++) {
		auto& image = images[i];
		if (!image.future.valid())
			continue;

		// Check if image has loaded
		if (image.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			continue;
		activeLoadThreads--;

		// Check for errors
		Image img = image.future.get();
		if (!img.Valid()) {
			images.erase(images.begin() + i);
			i--;
			std::string msg = "Cannot load " + image.path
				+ ".\nReason: " + img.Error() + ".";
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg.c_str(), GetWindow());
			continue;
		}
		
		// Create surface
		SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
			(void*)img.GetPixels(),
			img.GetWidth(),
			img.GetHeight(),
			32,
			img.GetWidth() * 4,
			SDL_PixelFormatEnum::SDL_PIXELFORMAT_ABGR8888);
		if (!surface)
			throw SDLException();

		// Create texture
		SDL_Texture* texture = SDL_CreateTextureFromSurface(GetRenderer(), surface);
		SDL_FreeSurface(surface);
		if (!texture)
			throw SDLException();
		
		image.image = std::move(img);
		image.texture = texture;

		ResetTransform(image);

		activeImageIndex = images.size() - 1;
	}

	// Begin loading images that haven't been loaded yet
	for (auto it = images.begin(); it != images.end(); ++it) {
		if (activeLoadThreads >= maxLoadThreads)
			break;

		// Skip if image is already open
		auto match = [&](const ImageEntity& im) { return im.path == it->path; };
		if (std::any_of(images.begin(), it, match)) {
			it = images.erase(it);
			if (it == images.end())
				break;
			continue;
		}
		
		if (!it->texture && !it->future.valid()) {
			it->future = std::async(std::launch::async, [path = it->path] {
				return Image(path.c_str());
				});
			activeLoadThreads++;
		}
	}
}

bool App::MouseOverSidebar() const {
	return GetMousePosition().first >= GetClientSize().first - SIDEBAR_WIDTH
		&& MouseInWindow();
}

bool App::TryGetCurrentImage(ImageEntity** image) {
	if (images.empty())
		return false;
	*image = &images[hoverImageIndex.value_or(activeImageIndex)];
	return true;
}

bool App::TryGetCurrentImage(const ImageEntity** image) const {
	if (images.empty())
		return false;
	*image = &images[hoverImageIndex.value_or(activeImageIndex)];
	return true;
}

bool App::TryGetVisibleImage(ImageEntity** image) {
	return TryGetCurrentImage(image) && (*image)->texture;
}

bool App::TryGetVisibleImage(const ImageEntity** image) const {
	return TryGetCurrentImage(image) && (*image)->texture;
}

void App::ResetTransform(ImageEntity& image) const {
	auto [cw, ch] = GetClientSize();
	float windowAspect = (float)cw / ch;
	float imgAspect = image.image.GetAspectRatio();
	if (imgAspect > windowAspect) {
		image.display.scale = (float)cw / image.image.GetWidth();
	} else {
		image.display.scale = (float)ch / image.image.GetHeight();
	}
	image.display.x = (cw - image.image.GetWidth() * image.display.scale) / 2;
	image.display.y = (ch - image.image.GetHeight() * image.display.scale) / 2;
}

SDL_Rect App::GetImageRect() const {
	const ImageEntity* image = nullptr;
	TryGetVisibleImage(&image);
	if (image->display.rotation % 2 == 0) {
		return {
			(int)image->display.x,
			(int)image->display.y,
			(int)(image->image.GetWidth() * image->display.scale),
			(int)(image->image.GetHeight() * image->display.scale)
		};
	} else {
		float w = image->image.GetWidth() * image->display.scale;
		float h = image->image.GetHeight() * image->display.scale;
		float centreX = image->display.x + w / 2;
		float centreY = image->display.y + h / 2;
		float x = image->display.x + w - centreX;
		float y = image->display.y - centreY;
		return {
			(int)(y + centreX),
			(int)(-x + centreY),
			(int)h,
			(int)w
		};
	}
}

void App::DrawGrid() const {
	const ImageEntity* image = nullptr;
	if (!TryGetVisibleImage(&image))
		return;

	const auto& display = image->display;
	if (display.scale < 2)
		return;

	auto [cw, ch] = GetClientSize();
	SDL_Rect rect = GetImageRect();
	float minX = std::max((float)rect.x, std::fmod((float)rect.x, display.scale));
	float maxX = (float)std::min(rect.x + rect.w, cw);
	float minY = std::max((float)rect.y, std::fmod((float)rect.y, display.scale));
	float maxY = (float)std::min(rect.y + rect.h, ch);
	
	std::vector<SDL_FPoint> points;
	for (float x = minX; x <= maxX + display.scale / 2; x += display.scale) {
		points.push_back({ x, minY });
		points.push_back({ x, (float)maxY });
		points.push_back({ x, minY });
	}
	for (float y = minY; y <= maxY + display.scale / 2; y += display.scale) {
		points.push_back({ minX, y });
		points.push_back({ maxX, y });
		points.push_back({ minX, y });
	}
	
	SDL_SetRenderDrawColor(GetRenderer(), 30, 30, 30, 255);
	SDL_RenderDrawLinesF(GetRenderer(), points.data(), (int)points.size());
}

void App::QueueFileLoad(std::string path) {
	std::error_code ec{};
	auto canonicalPath = std::filesystem::canonical(path, ec);
	if (!ec) {
		path = canonicalPath.string();
	}

	ImageEntity image{};
	image.path = std::move(path);
	images.push_back(std::move(image));
}

void App::ShowOpenFileDialog() {
	static const char* filters[] = {
		"*.jpeg", "*.jpg",
		"*.png", "*.bmp",
		"*.tga", "*.gif",
		"*.hdr", "*.psd",
		"*.pic", "*.pgm",
		"*.ppm",
	};
	const char* paths = tinyfd_openFileDialog(
		"Open File",
		nullptr,
		(int)std::size(filters),
		filters,
		nullptr,
		1);
	if (!paths)
		return;
	
	auto split = std::string(paths)
		| std::views::split('|')
		| std::views::transform([](auto&& s) { return std::string(s.begin(), s.end()); });
	for (const auto& path : split) {
		QueueFileLoad(path);
	}
}

void App::CloseFile(ImageEntity* image) {
	if (!image->texture) {
		auto heapFuture = new std::future<Image>(std::move(image->future));
		discardedFutures.push_back(heapFuture);
	}
	
	auto it = std::find_if(images.begin(), images.end(), [image](const ImageEntity& im) { return &im == image; });
	images.erase(it);

	if (activeImageIndex > 0 && activeImageIndex >= images.size()) {
		activeImageIndex = images.size() - 1;
	}
}
