#include "app.h"

App::App(int argc, char** argv) : Window(800, 600) {
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
}

void App::Render() const {
	SDL_RenderClear(GetRenderer());

	if (!images.empty() && images[0].texture) {
		SDL_RenderCopy(GetRenderer(), images[0].texture, nullptr, nullptr);
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

		int ec = SDL_UpdateTexture(
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