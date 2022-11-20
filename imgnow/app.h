#pragma once
#include "image.h"
#include "SDL.h"
#include <vector>
#include <future>

struct SDLException : std::exception {
	const char* what() const noexcept override;
};

struct App {
	App(int argc, char** argv);
	~App();
	void Run();
private:
	bool ProcessMessages();
	void Update();
	void Render() const;
	
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::vector<SDL_Texture*> imageTextures;
	std::vector<std::future<Bitmap>> imageFutures;
	bool quit = false;
};
