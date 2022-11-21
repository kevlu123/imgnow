#include "app.h"

App::App(int argc, char** argv) : Window(800, 600) {
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

void App::Update(float dt) {
	CheckImageFinishedLoading();
	
	auto [mx, my] = GetMousePosition();
	auto [_, sy] = GetScrollDelta();
	sy *= dt * 5;

	if (!images.empty()) {
		auto& image = images[activeImageIndex];
		if (image.image.Valid()) {
			// Zoom
			if (sy) {
				float& oldScale = image.transform.scale;
				float newScale = oldScale + sy * oldScale;
				image.transform.x = (image.transform.x - mx) / oldScale * newScale + mx;
				image.transform.y = (image.transform.y - my) / oldScale * newScale + my;
				oldScale = newScale;
			}

			// Begin drag
			else if (GetMousePressed(SDL_BUTTON_LEFT) || GetMousePressed(SDL_BUTTON_MIDDLE)) {
				dragLocation.x = mx;
				dragLocation.y = my;
			}

			// Continue drag
			else if (GetMouseDown(SDL_BUTTON_LEFT) || GetMouseDown(SDL_BUTTON_MIDDLE)) {
				image.transform.x += mx - dragLocation.x;
				image.transform.y += my - dragLocation.y;
				dragLocation.x = mx;
				dragLocation.y = my;
			}
		}
	}
}

void App::Render() const {
	SDL_RenderClear(GetRenderer());

	// Draw active image
	if (!images.empty()) {
		const auto& image = images[activeImageIndex];
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

	SDL_RenderPresent(GetRenderer());
}

void App::CheckImageFinishedLoading() {
	for (size_t i = 0; i < images.size(); i++) {
		auto& image = images[i];
		if (!image.future.valid())
			continue;

		if (image.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			continue;

		Image img = image.future.get();
		if (!img.Valid()) {
			images.erase(images.begin() + i);
			std::string msg = "Cannot load " + img.Path()
				+ ".\nReason: " + img.Error() + ".";
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg.c_str(), GetWindow());
			continue;
		}

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

		image.image = std::move(img);
		image.texture = tex;
	}
}
