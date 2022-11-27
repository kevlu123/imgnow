/*
 * TODO:
 * React to window resize.
 * Add app icon.
 * Reorder files.
 * Scroll bars.
 * Show alpha.
 * Clamp panning.
 */

#include "app.h"
#include <tuple>
#include <type_traits>
#include <ranges>
#include <filesystem>
#include <cmath>
#include <cstring> // memcpy

#include "tinyfiledialogs.h"
#include "clip.h"
	
constexpr int SIDEBAR_WIDTH = 100;
constexpr int SIDEBAR_BORDER = SIDEBAR_WIDTH / 10;
static const char* HELP_TEXT = R"(
imgnow
Copyright (c) 2022 Kevin Lu

==============================
 Shortcut		Action

 Ctrl+O		Open File
 Ctrl+W		Close File
 Ctrl+R		Reload From Disk
 Tab		Next Image
 Shift+Tab		Previous Image
 F1		Help
 F11		Fullscreen
 0-9		Switch to Image
 Q		Rotate Anti-clockwise
 W		Rotate 180 Degrees
 E		Rotate Clockwise
 F		Flip Horizontal
 V		Flip Vertical
 S		Toggle Sidebar
 Z		Reset Transform
 K		Switch Colour Format
 A		Toggle Alpha in Colour Format
==============================
)";

// Since std::future blocks when destroyed, we need to keep it
// alive until the image is loaded. We do this by moving the future into
// a heap allocated future which is periodically checked for completion.
// Being heap allocated means that the destructor does not have to
// be called when the program exits. This leak allows for quick termination.
static std::vector<std::future<Image>*> discardedFutures;

static SDL_Point ClampPoint(const SDL_Point& p, const SDL_Rect& rc) {
	return {
		std::clamp(p.x, rc.x, rc.x + rc.w),
		std::clamp(p.y, rc.y, rc.y + rc.h),
	};
}

static SDL_Rect RectFromPoints(const SDL_Point& p, const SDL_Point& q) {
	return {
		std::min(p.x, q.x),
		std::min(p.y, q.y),
		std::abs(p.x - q.x),
		std::abs(p.y - q.y),
	};
}

App::App(int argc, char** argv) : Window(1280, 720) {
	// Load config
	SDL_Rect windowRc{};
	if (config.TryGet("window_x", windowRc.x) && config.TryGet("window_y", windowRc.y)) {
		SDL_SetWindowPosition(GetWindow(), windowRc.x, windowRc.y);
	}
	if (config.TryGet("window_w", windowRc.w) && config.TryGet("window_h", windowRc.h)) {
		SDL_SetWindowSize(GetWindow(), windowRc.w, windowRc.h);
	}
	if (config.GetOr("maximized", false)) {
		SDL_MaximizeWindow(GetWindow());
	}
	SDL_ShowWindow(GetWindow());
	sidebarEnabled = config.GetOr("sidebar_enabled", true);
	colourFormatter.SetFormat(config.GetOr("colour_format", 0));
	colourFormatter.alphaEnabled = config.GetOr("colour_format_alpha", true);

	// Load images
	maxLoadThreads = std::max(1, (int)std::thread::hardware_concurrency() - 1);
	for (int i = 1; i < argc; i++) {
		QueueFileLoad(argv[i]);
	}
}

App::~App() {
	// Save config
	SDL_Rect windowRc{};
	SDL_GetWindowPosition(GetWindow(), &windowRc.x, &windowRc.y);
	SDL_GetWindowSize(GetWindow(), &windowRc.w, &windowRc.h);
	config.Set("window_x", windowRc.x);
	config.Set("window_y", windowRc.y);
	config.Set("window_w", windowRc.w);
	config.Set("window_h", windowRc.h);
	config.Set("maximized", maximized);
	config.Set("sidebar_enabled", sidebarEnabled);
	config.Set("colour_format", colourFormatter.GetFormat());
	config.Set("colour_format_alpha", colourFormatter.alphaEnabled);
	config.Save();

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
		// Open file
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_O)) {
			ShowOpenFileDialog();
		}
		
		// Close file
		else if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_W)) {
			ImageEntity* image = nullptr;
			if (TryGetCurrentImage(&image)) {
				DeleteImage(&*image);
			}
		}
	} else {
		// Toggle grid
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_G)) {
			gridEnabled = !gridEnabled;
		}

		// Show help
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F1)) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Help", HELP_TEXT, GetWindow());
		}

		// Toggle fullscreen
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F11)) {
			fullscreen = !fullscreen;
			SDL_SetWindowFullscreen(GetWindow(), fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		}

		// Switch colour format
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_K)) {
			colourFormatter.SwitchFormat();
		}

		// Toggle colour format alpha
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_A)) {
			colourFormatter.alphaEnabled = !colourFormatter.alphaEnabled;
		}

		// Switch image
		for (size_t i = 0; i < 10; i++) {
			if (GetKeyPressed((SDL_Scancode)(SDL_Scancode::SDL_SCANCODE_1 + i)) && i < images.size()) {
				activeImageIndex = i;
			}
		}
		
		// Next/previous image
		if (!images.empty() && GetKeyPressed(SDL_Scancode::SDL_SCANCODE_TAB)) {
			if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_LSHIFT) || GetKeyPressed(SDL_Scancode::SDL_SCANCODE_RSHIFT)) {
				activeImageIndex = (activeImageIndex + images.size() - 1) % images.size();
			} else {
				activeImageIndex = (activeImageIndex + 1) % images.size();
			}
		}
	}
	
	SDL_SetRenderDrawColor(GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(GetRenderer());

	UpdateActiveImage();	
	UpdateSidebar();
	UpdateStatus();

	SDL_RenderPresent(GetRenderer());
}

void App::Resized(int width, int height) {
	maximized = (SDL_GetWindowFlags(GetWindow()) & SDL_WINDOW_MAXIMIZED) != 0;
}

void App::UpdateStatus() const {
	const ImageEntity* image = nullptr;
	if (!TryGetVisibleImage(&image)) {
		SDL_SetWindowTitle(GetWindow(), "imgnow");
		return;
	}
	
	SDL_Point offset = ScreenToImagePosition(GetMousePosition());
	SDL_Rect bounds = { 0, 0, image->image.GetWidth(), image->image.GetHeight() };
	SDL_Colour colour = SDL_PointInRect(&offset, &bounds) ? image->image.GetPixel(offset.x, offset.y) : SDL_Colour{};

	static const std::string SEP = "  |  ";
	std::string text = "imgnow" + SEP
		+ image->path + SEP
		+ "Dim: " + std::to_string(image->image.GetWidth()) + "x" + std::to_string(image->image.GetHeight()) + SEP
		+ "XY: (" + std::to_string(offset.x) + ", " + std::to_string(offset.y) + ")" + SEP
		+ colourFormatter.GetLabel() + ": " + colourFormatter.FormatColour(colour) + SEP
		+ "Channels: " + std::to_string(image->image.GetChannels()) + SEP
		+ "Zoom: " + std::to_string((int)(image->display.scale * 100)) + "%";
	SDL_SetWindowTitle(GetWindow(), text.c_str());

	// Copy colour to clipboard
	if (GetCtrlKeyDown() && GetKeyPressed(SDL_Scancode::SDL_SCANCODE_K)) {
		if (!clip::set_text(colourFormatter.FormatColour(colour))) {
			SDL_ShowSimpleMessageBox(
				SDL_MESSAGEBOX_ERROR,
				"Clipboard Error",
				"Failed to copy colour to clipboard.",
				GetWindow());
		}
	}
}

void App::UpdateActiveImage() {
	ImageEntity* image = nullptr;
	if (!TryGetVisibleImage(&image))
		return;

	auto& display = image->display;

	auto [mx, my] = GetMousePosition();
	auto [_, sy] = GetScrollDelta();
	sy *= GetDeltaTime() * 5;

	bool dragged = false;

	// Zoom
	if (sy && !MouseOverSidebar()) {
		float& oldScale = display.scale;
		float newScale = oldScale + sy * oldScale;
		newScale = std::min(newScale, 512.0f);
		if (newScale > 0) {
			display.x = (display.x - mx) / oldScale * newScale + mx;
			display.y = (display.y - my) / oldScale * newScale + my;
			oldScale = newScale;
		}
	}

	// Begin drag
	else if ((GetMousePressed(SDL_BUTTON_LEFT) || GetMousePressed(SDL_BUTTON_MIDDLE)) && !MouseOverSidebar()) {
		dragged = true;
		dragLocation = { mx, my };
	}

	// Continue drag
	else if (GetMouseDown(SDL_BUTTON_LEFT) || GetMouseDown(SDL_BUTTON_MIDDLE)) {
		if (dragLocation) {
			dragged = true;
			display.x += mx - dragLocation.value().x;
			display.y += my - dragLocation.value().y;
			dragLocation.value().x = mx;
			dragLocation.value().y = my;
		}
	}

	// Begin select
	else if (GetMousePressed(SDL_BUTTON_RIGHT) && !MouseOverSidebar()) {
		SDL_Rect rc = { 0, 0, image->image.GetWidth() - 1, image->image.GetHeight() - 1 };
		display.selectFrom = ClampPoint(ScreenToImagePosition({ mx, my }), rc);
		display.selectTo = { -1, -1 };
	}

	// Continue select
	else if (GetMouseDown(SDL_BUTTON_RIGHT)) {
		auto [mdx, mdy] = GetMouseDelta();
		if (display.selectTo.x != -1 || std::abs(mdx) + std::abs(mdy) > 0) {
			SDL_Rect rc = { 0, 0, image->image.GetWidth() - 1, image->image.GetHeight() - 1 };
			display.selectTo = ClampPoint(ScreenToImagePosition({ mx, my }), rc);
		}
	}

	// Deselect
	else if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_ESCAPE)) {
		display.selectFrom = { -1, -1 };
		display.selectTo = { -1, -1 };
	}

	// Reload
	else if (GetCtrlKeyDown() && GetKeyDown(SDL_Scancode::SDL_SCANCODE_R)) {
		SDL_DestroyTexture(image->texture);
		image->texture = nullptr;
		image->image = Image();
	}
	
	else if (!GetCtrlKeyDown()) {
		// Reset transform
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_Z)) {
			ResetTransform(*image);
		}

		// Flipping:
		// Since flipping is applied before rotation for SDL_RenderCopyEx,
		// we need to take into account the current rotation to toggle the correct flag.
		bool flipH = GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F) && !RotatedPerpendicular()
			|| GetKeyPressed(SDL_Scancode::SDL_SCANCODE_V) && RotatedPerpendicular();
		bool flipV = GetKeyPressed(SDL_Scancode::SDL_SCANCODE_F) && RotatedPerpendicular()
			|| GetKeyPressed(SDL_Scancode::SDL_SCANCODE_V) && !RotatedPerpendicular();
		if (flipH) {
			display.flipHorizontal = !display.flipHorizontal;
		}
		if (flipV) {
			display.flipVertical = !display.flipVertical;
		}

		// Rotate clockwise
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_E)) {
			display.rotation = (display.rotation + 1) % 4;
		}

		// Rotate 180 degrees
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_W)) {
			display.rotation = (display.rotation + 2) % 4;
		}

		// Rotate anti-clockwise
		if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_Q)) {
			display.rotation = (display.rotation + 3) % 4;
		}
	}

	// End drag
	if (!dragged) {
		dragLocation = std::nullopt;
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
	if (image->image.GetChannels() == 4 && display.animatedRotation == display.rotation) {
		DrawAlphaBackground();
	}
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

	// Draw grid
	if (gridEnabled) {
		DrawGrid();
	}

	// Draw selection
	if (display.selectTo.x != -1) {
		SDL_Rect selection = RectFromPoints(display.selectFrom, display.selectTo);
		SDL_Point topLeft = ImageToScreenPosition({ selection.x, selection.y });
		SDL_Point bottomRight = ImageToScreenPosition(
			{ selection.x + selection.w + 1,
			selection.y + selection.h + 1 });
		SDL_Rect dst = RectFromPoints(topLeft, bottomRight);
		SDL_SetRenderDrawColor(GetRenderer(), 0, 0, 0, 100);
		SDL_RenderFillRect(GetRenderer(), &dst);
		SDL_SetRenderDrawColor(GetRenderer(), 200, 200, 200, 200);
		SDL_RenderDrawRect(GetRenderer(), &dst);

		if (GetCtrlKeyDown() && GetKeyPressed(SDL_Scancode::SDL_SCANCODE_C)) {
			CopyToClipboard();
		}
	}
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
			SDL_Point mp = GetMousePosition();
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
		sidebarScroll = std::clamp(sidebarScroll, 0.0f, y);
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
			std::string msg = "Cannot load " + image.path
				+ ".\nReason: " + img.Error() + ".";
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg.c_str(), GetWindow());
			DeleteImage(images.data() + i);
			i--;
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

		activeImageIndex = i;
	}

	// Begin loading images that haven't been loaded yet
	for (auto it = images.begin(); it != images.end(); ++it) {
		if (!it->texture && !it->future.valid()) {
			// Skip if image is already open
			auto match = [&](const ImageEntity& im) { return im.path == it->path; };
			if (std::any_of(images.begin(), it, match)) {
				it = DeleteImage(&*it);
				if (it == images.end())
					break;
				continue;
			}

			if (activeLoadThreads >= maxLoadThreads)
				break;

			it->future = std::async(std::launch::async, [path = it->path] {
				return Image(path.c_str());
				});
			activeLoadThreads++;
		}
	}
}

std::vector<ImageEntity>::iterator App::DeleteImage(ImageEntity* image) {
	if (image->future.valid()) {
		auto heapFuture = new std::future<Image>(std::move(image->future));
		discardedFutures.push_back(heapFuture);
	}

	auto it = std::find_if(images.begin(), images.end(), [image](const ImageEntity& im) { return &im == image; });
	it = images.erase(it);

	if (activeImageIndex > 0 && activeImageIndex >= images.size()) {
		activeImageIndex = images.size() - 1;
	}
	
	return it;
}

bool App::MouseOverSidebar() const {
	return GetMousePosition().x >= GetClientSize().x - SIDEBAR_WIDTH
		&& sidebarEnabled
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
	if (RotatedPerpendicular()) {
		imgAspect = 1.0f / imgAspect;
	}

	bool rotated = RotatedPerpendicular();
	if (imgAspect > windowAspect) {
		int w = rotated ? image.image.GetHeight() : image.image.GetWidth();
		image.display.scale = (float)cw / w;
	} else {
		int h = rotated ? image.image.GetWidth() : image.image.GetHeight();
		image.display.scale = (float)ch / h;
	}

	image.display.x = (cw - image.image.GetWidth() * image.display.scale) / 2;
	image.display.y = (ch - image.image.GetHeight() * image.display.scale) / 2;
}

SDL_Rect App::GetImageRect() const {
	const ImageEntity* image = nullptr;
	TryGetVisibleImage(&image);
	if (!RotatedPerpendicular()) {
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
		points.push_back({ x, maxY });
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
	
	std::string pathsStr = paths;
	auto split = pathsStr
		| std::views::split('|')
		| std::views::transform([](auto&& s) {
			return std::string_view(&*s.begin(), std::ranges::distance(s));
			});
	for (const auto& path : split) {
		QueueFileLoad(std::string(path));
	}
}

SDL_Point App::ScreenToImagePosition(SDL_Point p) const {
	const ImageEntity* image = nullptr;
	TryGetVisibleImage(&image);
	SDL_Rect rect = GetImageRect();
	
	SDL_Point offset = {
		(int)std::floor((p.x - rect.x) / image->display.scale),
		(int)std::floor((p.y - rect.y) / image->display.scale)
	};

	SDL_Point flipped = offset;
	if (image->display.flipHorizontal) {
		int w = !RotatedPerpendicular() ? image->image.GetWidth() : image->image.GetHeight();
		flipped.x = w - offset.x - 1;
	}
	if (image->display.flipVertical) {
		int h = RotatedPerpendicular() ? image->image.GetWidth() : image->image.GetHeight();
		flipped.y = h - offset.y - 1;
	}

	SDL_Point rotated{};
	int rot = image->display.rotation;
	if (image->display.flipHorizontal != image->display.flipVertical) {
		rot = (4 - rot) % 4;
	}
	switch (rot) {
	case 0:
		rotated = flipped;
		break;
	case 1:
		rotated.x = flipped.y;
		rotated.y = image->image.GetHeight() - flipped.x - 1;
		break;
	case 2:
		rotated.x = image->image.GetWidth() - flipped.x - 1;
		rotated.y = image->image.GetHeight() - flipped.y - 1;
		break;
	case 3:
		rotated.x = image->image.GetWidth() - flipped.y - 1;
		rotated.y = flipped.x;
		break;
	default:
		std::abort();
	}
	
	return rotated;
}

SDL_Point App::ImageToScreenPosition(SDL_Point p) const {
	const ImageEntity* image = nullptr;
	TryGetVisibleImage(&image);
	SDL_Rect rect = GetImageRect();

	SDL_Point unrotated{};
	int rot = image->display.rotation;
	if (image->display.flipHorizontal != image->display.flipVertical) {
		rot = (4 - rot) % 4;
	}
	switch (rot) {
	case 0:
		unrotated = p;
		break;
	case 1:
		unrotated.x = image->image.GetHeight() - p.y;
		unrotated.y = p.x;
		break;
	case 2:
		unrotated.x = image->image.GetWidth() - p.x;
		unrotated.y = image->image.GetHeight() - p.y;
		break;
	case 3:
		unrotated.x = p.y;
		unrotated.y = image->image.GetWidth() - p.x;
		break;
	default:
		std::abort();
	}

	SDL_Point unflipped = unrotated;
	if (image->display.flipHorizontal) {
		int w = !RotatedPerpendicular() ? image->image.GetWidth() : image->image.GetHeight();
		unflipped.x = w - unrotated.x;
	}
	if (image->display.flipVertical) {
		int h = RotatedPerpendicular() ? image->image.GetWidth() : image->image.GetHeight();
		unflipped.y = h - unrotated.y;
	}
	
	return {
		(int)std::floor(unflipped.x * image->display.scale + rect.x),
		(int)std::floor(unflipped.y * image->display.scale + rect.y)
	};
}

void App::CopyToClipboard() const {
	const ImageEntity* image = nullptr;
	TryGetVisibleImage(&image);
	const auto& display = image->display;

	SDL_Rect rect = {
		std::min(display.selectFrom.x, display.selectTo.x),
		std::min(display.selectFrom.y, display.selectTo.y),
		std::abs(display.selectFrom.x - display.selectTo.x) + 1,
		std::abs(display.selectFrom.y - display.selectTo.y) + 1,
	};
	
	std::vector<uint8_t> data;
	const uint8_t* fullImage = image->image.GetPixels();
	for (int dy = 0; dy < rect.h; dy++) {
		const uint8_t* p = fullImage + 4 * (image->image.GetWidth() * (rect.y + dy) + rect.x);
		data.insert(data.end(), p, p + 4 * rect.w);
	}

	std::vector<uint8_t> transformed;
	if (!display.flipHorizontal && !display.flipVertical && display.rotation == 0) {
		transformed = std::move(data);
	} else {
		transformed.resize(data.size());
		int dstW = !RotatedPerpendicular() ? rect.w : rect.h;
		int dstH = RotatedPerpendicular() ? rect.w : rect.h;
		for (int y = 0; y < rect.h; y++) {
			for (int x = 0; x < rect.w; x++) {
				int srcX = x;
				int srcY = y;
				if (display.flipHorizontal) {
					srcX = rect.w - x - 1;
				}
				if (display.flipVertical) {
					srcY = rect.h - y - 1;
				}

				int dstX = x;
				int dstY = y;
				switch (display.rotation) {
				case 0:
					break;
				case 1:
					dstX = rect.h - y - 1;
					dstY = x;
					break;
				case 2:
					dstX = rect.w - x - 1;
					dstY = rect.h - y - 1;
					break;
				case 3:
					dstX = y;
					dstY = rect.w - x - 1;
					break;
				}

				std::memcpy(
					transformed.data() + 4 * (dstY * dstW + dstX),
					data.data() + 4 * (srcY * rect.w + srcX),
					4);
			}
		}

		// Update the copied image dimensions
		rect.w = dstW;
		rect.h = dstH;
	}
	
	clip::image_spec spec{};
	spec.alpha_mask = 0xFF000000;
	spec.blue_mask =  0x00FF0000;
	spec.green_mask = 0x0000FF00;
	spec.red_mask =   0x000000FF;
	spec.alpha_shift = 24;
	spec.blue_shift = 16;
	spec.green_shift = 8;
	spec.red_shift = 0;
	spec.width = rect.w;
	spec.height = rect.h;
	spec.bits_per_pixel = 32;
	spec.bytes_per_row = spec.bits_per_pixel / 8 * spec.width;
	
	clip::image clipimage(transformed.data(), spec);
	
	if (!clip::set_image(clipimage)) {
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"Clipboard Error",
			"Failed to copy image to clipboard.",
			GetWindow());
	}
}

bool App::RotatedPerpendicular() const {
	const ImageEntity* image = nullptr;
	TryGetVisibleImage(&image);
	return image->display.rotation % 2 == 1;
}

void App::DrawAlphaBackground() const {
	SDL_Rect rc = GetImageRect();

	rc.x += 1;
	rc.y += 1;
	rc.w -= 2;
	rc.h -= 2;

	if (!SDL_RectEquals(&rc, &alphaBgCachedRect)) {
		constexpr int SQUARE_SIZE = 8;
		auto [cw, ch] = GetClientSize();
		int xmin = std::max(0, rc.x / SQUARE_SIZE * SQUARE_SIZE);
		int ymin = std::max(0, rc.y / SQUARE_SIZE * SQUARE_SIZE);
		int xmax = std::min(cw, (rc.x + rc.w) / SQUARE_SIZE * SQUARE_SIZE);
		int ymax = std::min(ch, (rc.y + rc.h) / SQUARE_SIZE * SQUARE_SIZE);

		std::vector<SDL_Rect> squares;
		int i = (xmin / SQUARE_SIZE + ymin / SQUARE_SIZE) % 2;
		for (int y = ymin, yi = i; y <= ymax; y += SQUARE_SIZE, yi++) {
			for (int x = xmin, xi = 0; x <= xmax; x += SQUARE_SIZE, xi++) {
				if ((xi + yi) % 2 == 0) {
					int left = std::max(rc.x, x);
					int top = std::max(rc.y, y);
					int right = std::min(x + SQUARE_SIZE, rc.x + rc.w);
					int bottom = std::min(y + SQUARE_SIZE, rc.y + rc.h);
					squares.push_back({
						left,
						top,
						right - left,
						bottom - top,
						});
				}
			}
		}

		alphaBgCachedRect = rc;
		alphaBgCachedSquares = std::move(squares);
	}

	SDL_SetRenderDrawColor(GetRenderer(), 255, 255, 255, 255);
	SDL_RenderFillRect(GetRenderer(), &rc);
	SDL_SetRenderDrawColor(GetRenderer(), 191, 191, 191, 255);
	SDL_RenderFillRects(GetRenderer(), alphaBgCachedSquares.data(), (int)alphaBgCachedSquares.size());
}
