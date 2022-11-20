#include "app.h"
#include "image.h"

const char* SDLException::what() const noexcept {
	return SDL_GetError();
}

App::App(int argc, char** argv) {
	// Begin loading images asynchronously
	for (int i = 0; i < argc; i++) {
		auto future = std::async(std::launch::async, LoadImage, argv[i]);
		imageFutures.push_back(std::move(future));
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
	for (SDL_Texture* tex : imageTextures) {
		SDL_DestroyTexture(tex);
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
	
}

void App::Render() const {
	SDL_RenderClear(renderer);
	
	SDL_RenderPresent(renderer);
}
