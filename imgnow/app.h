#pragma once
#include "image.h"
#include "SDL.h"
#include <vector>
#include <future>
#include <optional>

struct SDLException : std::exception {
	const char* what() const noexcept override;
};

struct ImageEntity {
	std::future<Image> future;
	std::optional<Image> image;
	SDL_Texture* texture;
};

struct App {
	App(int argc, char** argv);
	~App();
	void Run();
private:
	bool ProcessMessages();
	void Update();
	void Render() const;
	void CheckImageFinishedLoading();
	
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::vector<ImageEntity> images;
	bool quit = false;
};
