#pragma once
#include "image.h"
#include "SDL.h"
#include <Windows.h>
#include <vector>
#include <future>
#include <unordered_map>

struct SDLException : std::exception {
	const char* what() const noexcept override;
};

enum class KeyState {
	Up, Pressed, Down, Released,
};

struct Window {
	Window(int width, int height);
	~Window();
	void Run();
	virtual void Update();
	virtual void Render() const;
	bool GetKeyDown(SDL_Scancode key) const;
	bool GetKeyPressed(SDL_Scancode key) const;
	bool GetKeyReleased(SDL_Scancode key) const;
	SDL_Window* GetWindow() const;
	SDL_Renderer* GetRenderer() const;
	HWND GetHwnd() const;
private:
	bool ProcessMessages();
	void UpdateInput();

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	HWND hwnd = nullptr;
	bool quit = false;
	std::unordered_map<SDL_Scancode, KeyState> keyStates;
};
