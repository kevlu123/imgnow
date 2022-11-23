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
 * Open file.
 * Close file.
 * Reorder files.
 * Add linux build script.
*/

#include "app.h"
#include <tuple>
#include <type_traits>
	
constexpr int SIDEBAR_WIDTH = 100;
constexpr int SIDEBAR_BORDER = SIDEBAR_WIDTH / 10;
static const char* HELP_TEXT = R"(
imgnow
Copyright (c) 2022 Kevin Lu

==============================
 Shortcut		Action

 F1		Help
 0..9		Switch to Image
 Q		Rotate Anti-clockwise
 W		Rotate 180 Degrees
 E		Rotate Clockwise
 T		Reset Zoom
 S		Toggle Sidebar
 F		Flip Horizontal
 V		Flip Vertical
==============================
)";

App::App(int argc, char** argv) : Window(1280, 720) {
	// Begin loading images asynchronously
	for (int i = 1; i < argc; i++) {
		auto future = std::async(std::launch::async, [i, argv] { return Image(argv[i]); });
		ImageEntity image{};
		image.future = std::move(future);
		images.push_back(std::move(image));
	}
}

App::~App() {
	for (auto& image : images) {
		if (image.texture) {
			SDL_DestroyTexture(image.texture);
		}
	}
}

void App::Update() {
	CheckImageFinishedLoading();

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
		SDL_SetWindowFullscreen(GetWindow(), fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP: 0);
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
	
	SDL_Point offset{};
	offset.x = (int)std::floor((mx - rect.x) / image->display.scale);
	offset.y = (int)std::floor((my - rect.y) / image->display.scale);
	
	uint32_t colour = SDL_PointInRect(&offset, &rect) ? image->image.GetPixel(offset.x, offset.y) : 0x00000000;
	char hexColour[9]{};
	for (int i = 0; i < 8; i++) {
		hexColour[i] = "0123456789ABCDEF"[colour >> (28 - i * 4) & 0xF];
	}

	static const std::string SEP = "  |  ";
	std::string text = "imgnow" + SEP
		+ image->image.Path() + SEP
		+ "Dim: " + std::to_string(image->image.GetWidth()) + "x" + std::to_string(image->image.GetHeight()) + SEP
		+ "XY: (" + std::to_string(offset.x) + ", " + std::to_string(offset.y) + ")" + SEP
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

	if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_T)) {
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
	if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_E)) {
		display.rotation = (display.rotation + 1) % 4;
	}

	// Rotate 180 degrees
	if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_W)) {
		display.rotation = (display.rotation + 2) % 4;
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
	if (!SidebarVisible())
		return;

	auto [cw, ch] = GetClientSize();
	auto [_, sy] = GetScrollDelta();

	// Toggle sidebar
	if (GetKeyPressed(SDL_Scancode::SDL_SCANCODE_S)) {
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

void App::CheckImageFinishedLoading() {
	for (size_t i = 0; i < images.size(); i++) {
		auto& image = images[i];
		if (!image.future.valid())
			continue;

		// Check if image has loaded
		if (image.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			continue;

		// Check for errors
		Image img = image.future.get();
		if (!img.Valid()) {
			images.erase(images.begin() + i);
			std::string msg = "Cannot load " + img.Path()
				+ ".\nReason: " + img.Error() + ".";
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg.c_str(), GetWindow());
			continue;
		}

		// Create texture
		SDL_Texture* tex = SDL_CreateTexture(
			GetRenderer(),
			SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STATIC,
			img.GetWidth(),
			img.GetHeight());
		if (!tex)
			throw SDLException();
		
		int ec = SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
		if (ec)
			throw SDLException();

		ec = SDL_UpdateTexture(
			tex,
			nullptr,
			img.GetPixels(),
			img.GetWidth() * 4);
		if (ec)
			throw SDLException();
		
		image.image = std::move(img);
		image.texture = tex;

		ResetTransform(image);
	}
}

bool App::SidebarVisible() const {
	return images.size() > 1;
}

bool App::MouseOverSidebar() const {
	return GetMousePosition().first >= GetClientSize().first - SIDEBAR_WIDTH
		&& MouseInWindow();
}

bool App::TryGetVisibleImage(ImageEntity** image) {
	if (images.empty())
		return false;
	auto& im = images[hoverImageIndex.value_or(activeImageIndex)];
	if (!im.texture)
		return false;
	*image = &im;
	return true;
}

bool App::TryGetVisibleImage(const ImageEntity** image) const {
	if (images.empty())
		return false;
	auto& im = images[hoverImageIndex.value_or(activeImageIndex)];
	if (!im.texture)
		return false;
	*image = &im;
	return true;
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
