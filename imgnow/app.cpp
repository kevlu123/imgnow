#include "app.h"
#include "image.h"

const char* SDLException::what() const noexcept {
	return SDL_GetError();
}

App::App(int argc, char** argv) {
	// Begin loading images asynchronously
	for (int i = 1; i < argc; i++) {
		auto future = std::async(std::launch::async, [i, argv] { return Image(argv[i]); });
		ImageEntity image{};
		image.future = std::move(future);
		images.push_back(std::move(image));
	}

	// Initialize SDL
	int ec = SDL_Init(SDL_INIT_VIDEO);
	if (ec)
		throw SDLException();

	ec = SDL_CreateWindowAndRenderer(
		800, 600,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN,
		&window,
		&renderer);
	if (ec)
		throw SDLException();

	ec = SDL_RenderSetVSync(renderer, 1);
	if (ec)
		throw SDLException();
}

App::~App() {
	for (auto& image : images) {
		if (image.texture) {
			SDL_DestroyTexture(image.texture);
		}
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

bool App::ProcessMessages() {
	SDL_Event ev{};
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			return false;
		}
	}
	return true;
}

void App::Run() {
	while (ProcessMessages()) {
		Update();
		Render();
	}
}

void App::Update() {
	CheckImageFinishedLoading();
}

void App::Render() const {
	SDL_RenderClear(renderer);

	if (!images.empty() && images[0].texture) {
		SDL_RenderCopy(renderer, images[0].texture, nullptr, nullptr);
	}

	SDL_RenderPresent(renderer);
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
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg.c_str(), window);
			continue;
		}

		SDL_Texture* tex = SDL_CreateTexture(
			renderer,
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
