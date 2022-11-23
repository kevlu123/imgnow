#pragma once
#include "image.h"
#include "SDL.h"
#include <Windows.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <future>

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
	bool GetKeyDown(SDL_Scancode key) const;
	bool GetKeyPressed(SDL_Scancode key) const;
	bool GetKeyReleased(SDL_Scancode key) const;
	bool GetMouseDown(int button) const;
	bool GetMousePressed(int button) const;
	bool GetMouseReleased(int button) const;
	std::pair<float, float> GetScrollDelta() const;
	std::pair<int, int> GetMousePosition() const;
	bool MouseInWindow() const;
	std::pair<int, int> GetClientSize() const;
	SDL_Window* GetWindow() const;
	SDL_Renderer* GetRenderer() const;
	HWND GetHwnd() const;
	float GetDeltaTime() const;
private:
	bool ProcessMessages();
	void UpdateInput();

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	HWND hwnd = nullptr;
	bool quit = false;
	std::unordered_map<SDL_Scancode, KeyState> keyStates;
	std::array<KeyState, 5> mouseStates{};
	std::pair<float, float> scrollDelta{};
	std::pair<int, int> mousePosition{};
	bool mouseInWindow = false;
	float deltaTime = 0;
};

static_assert(SDL_BUTTON_LEFT == 1);
static_assert(SDL_BUTTON_MIDDLE == 2);
static_assert(SDL_BUTTON_RIGHT == 3);
static_assert(SDL_BUTTON_X1 == 4);
static_assert(SDL_BUTTON_X2 == 5);
