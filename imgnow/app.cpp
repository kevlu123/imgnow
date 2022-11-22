#include "app.h"
#include <tuple>
	
constexpr int SIDEBAR_WIDTH = 100;
constexpr int SIDEBAR_BORDER = SIDEBAR_WIDTH / 10;

App::App(int argc, char** argv) : Window(1280, 720) {
	// Begin loading images asynchronously
	for (int i = 1; i < argc; i++) {
		auto future = std::async(std::launch::async, [i, argv] { return Image(argv[i]); });
		ImageEntity image{};
		image.future = std::move(future);
		images.push_back(std::move(image));
	}
	
	if (argc <= 1) {
		SDL_SetWindowTitle(GetWindow(), "imgnow");
	} else {
		SDL_SetWindowTitle(GetWindow(), argv[1]);
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

	SDL_SetRenderDrawColor(GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(GetRenderer());

	UpdateActiveImage();
	UpdateSidebar();

	SDL_RenderPresent(GetRenderer());
}

void App::UpdateActiveImage() {
	auto [cw, ch] = GetClientSize();
	auto [mx, my] = GetMousePosition();
	auto [_, sy] = GetScrollDelta();
	sy *= GetDeltaTime() * 5;

	if (!images.empty()) {
		auto& image = images[activeImageIndex];
		if (image.image.Valid()) {
			// Zoom
			if (sy && !MouseOverSidebar()) {
				float& oldScale = image.transform.scale;
				float newScale = oldScale + sy * oldScale;
				image.transform.x = (image.transform.x - mx) / oldScale * newScale + mx;
				image.transform.y = (image.transform.y - my) / oldScale * newScale + my;
				oldScale = newScale;
			}

			// Begin drag
			else if ((GetMousePressed(SDL_BUTTON_LEFT) || GetMousePressed(SDL_BUTTON_MIDDLE)) && !MouseOverSidebar()) {
				dragLocation = { mx, my };
			}

			// Continue drag
			else if (GetMouseDown(SDL_BUTTON_LEFT) || GetMouseDown(SDL_BUTTON_MIDDLE)) {
				if (dragLocation) {
					image.transform.x += mx - dragLocation.value().x;
					image.transform.y += my - dragLocation.value().y;
					dragLocation.value().x = mx;
					dragLocation.value().y = my;
				}
			}

			// End drag
			else {
				dragLocation = std::nullopt;
			}
		}
	}

	// Draw active image
	if (!images.empty()) {
		const auto& image = images[hoverImageIndex.value_or(activeImageIndex)];
		if (image.image.Valid()) {
			SDL_Rect dst{};
			dst.x = (int)image.transform.x;
			dst.y = (int)image.transform.y;
			dst.w = (int)(image.transform.scale * image.image.GetWidth());
			dst.h = (int)(image.transform.scale * image.image.GetHeight());

			SDL_RendererFlip flip{};
			if (image.transform.flipHorizontal)
				SDL_RendererFlip::SDL_FLIP_HORIZONTAL;
			if (image.transform.flipVertical)
				SDL_RendererFlip::SDL_FLIP_VERTICAL;

			SDL_RenderCopyEx(
				GetRenderer(),
				image.texture,
				nullptr,
				&dst,
				90 * image.transform.rotation,
				nullptr,
				flip);
		}
	}
}

void App::UpdateSidebar() {
	if (!SidebarVisible())
		return;

	auto [cw, ch] = GetClientSize();
	auto [_, sy] = GetScrollDelta();

	// Draw background
	SDL_Rect sbRc{};
	sbRc.x = cw - SIDEBAR_WIDTH;
	sbRc.w = SIDEBAR_WIDTH;
	sbRc.h = ch;
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

		// Highlight if image is active
		if (activeImageIndex == i) {
			SDL_SetRenderDrawColor(GetRenderer(), 255, 255, 255, 255);
			SDL_RenderDrawRect(GetRenderer(), &rc);
		}

		// Highlight if cursor is over icon
		else {
			SDL_Point mp{};
			std::tie(mp.x, mp.y) = GetMousePosition();

			SDL_Rect hitbox{};
			hitbox.x = sbRc.x;
			hitbox.y = (int)screenY + SIDEBAR_BORDER / 2;
			hitbox.w = sbRc.w;
			hitbox.h = rc.h + SIDEBAR_BORDER;
			if (SDL_PointInRect(&mp, &hitbox)) {
				SDL_SetRenderDrawColor(GetRenderer(), 150, 150, 150, 255);
				SDL_RenderDrawRect(GetRenderer(), &rc);
				hoverImageIndex = i;
				if (GetMousePressed(SDL_BUTTON_LEFT)) {
					// Selected a different image
					activeImageIndex = i;
					SDL_SetWindowTitle(GetWindow(), image.image.Path().c_str());
				}
			}
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
			img.GetPixels().data(),
			img.GetWidth() * 4);
		if (ec)
			throw SDLException();

		// Set transform
		auto [cw, ch] = GetClientSize();
		float windowAspect = (float)cw / ch;
		float imgAspect = img.GetAspectRatio();
		if (imgAspect > windowAspect) {
			image.transform.scale = (float)cw / img.GetWidth();
		} else {
			image.transform.scale = (float)ch / img.GetHeight();
		}
		image.transform.x = (cw - img.GetWidth() * image.transform.scale) / 2;
		image.transform.y = (ch - img.GetHeight() * image.transform.scale) / 2;

		// Store image and texture to be accessed later
		image.image = std::move(img);
		image.texture = tex;
	}
}

bool App::SidebarVisible() const {
	return images.size() > 1;
}

bool App::MouseOverSidebar() const {
	return GetMousePosition().first >= GetClientSize().first - SIDEBAR_WIDTH;
}
