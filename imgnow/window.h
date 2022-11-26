#pragma once
#include "image.h"
#include "SDL.h"
#include <vector>
#include <array>
#include <unordered_map>
#include <future>
#include <memory>

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
	virtual void Resized(int width, int height);
	bool GetKeyDown(SDL_Scancode key) const;
	bool GetKeyPressed(SDL_Scancode key) const;
	bool GetKeyReleased(SDL_Scancode key) const;
	bool GetCtrlKeyDown() const;
	bool GetMouseDown(int button) const;
	bool GetMousePressed(int button) const;
	bool GetMouseReleased(int button) const;
	SDL_FPoint GetScrollDelta() const;
	SDL_Point GetMousePosition() const;
	SDL_Point GetMouseDelta() const;
	bool MouseInWindow() const;
	SDL_Point GetClientSize() const;
	SDL_Window* GetWindow() const;
	SDL_Renderer* GetRenderer() const;
	float GetDeltaTime() const;
private:
	bool ProcessMessages();
	void UpdateInput();

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	bool quit = false;
	std::unordered_map<SDL_Scancode, KeyState> keyStates;
	std::array<KeyState, 5> mouseStates{};
	SDL_FPoint scrollDelta{};
	SDL_Point mousePosition{};
	SDL_Point mouseDelta{};
	bool mouseInWindow = false;
	float deltaTime = 0;
};

static_assert(SDL_BUTTON_LEFT == 1);
static_assert(SDL_BUTTON_MIDDLE == 2);
static_assert(SDL_BUTTON_RIGHT == 3);
static_assert(SDL_BUTTON_X1 == 4);
static_assert(SDL_BUTTON_X2 == 5);
