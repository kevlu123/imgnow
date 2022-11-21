#include "window.h"
#include "image.h"
#include "SDL_syswm.h"

const char* SDLException::what() const noexcept {
	return SDL_GetError();
}

Window::Window(int width, int height) {
	// Initialize SDL
	int ec = SDL_Init(SDL_INIT_VIDEO);
	if (ec)
		throw SDLException();

	ec = SDL_CreateWindowAndRenderer(
		width, height,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN,
		&window,
		&renderer);
	if (ec)
		throw SDLException();

	ec = SDL_RenderSetVSync(renderer, 1);
	if (ec)
		throw SDLException();

	// Get native window handle
	SDL_SysWMinfo wmInfo{};
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	hwnd = wmInfo.info.win.window;
}

Window::~Window() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

bool Window::ProcessMessages() {
	SDL_Event ev{};
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			return false;
		case SDL_KEYDOWN:
			keyStates[ev.key.keysym.scancode] = KeyState::Pressed;
			break;
		case SDL_KEYUP:
			keyStates[ev.key.keysym.scancode] = KeyState::Released;
			break;
		}
	}
	return true;
}

void Window::UpdateInput() {
	for (const auto& pair : keyStates) {
		if (pair.second == KeyState::Pressed) {
			keyStates[pair.first] = KeyState::Down;
		} else if (pair.second == KeyState::Released) {
			keyStates[pair.first] = KeyState::Up;
		}
	}
}

bool Window::GetKeyDown(SDL_Scancode key) const {
	auto it = keyStates.find(key);
	return it != keyStates.end() && (it->second == KeyState::Pressed || it->second == KeyState::Down);
}

bool Window::GetKeyPressed(SDL_Scancode key) const {
	auto it = keyStates.find(key);
	return it != keyStates.end() && it->second == KeyState::Pressed;
}

bool Window::GetKeyReleased(SDL_Scancode key) const {
	auto it = keyStates.find(key);
	return it != keyStates.end() && it->second == KeyState::Released;
}

SDL_Window* Window::GetWindow() const {
	return window;
}

SDL_Renderer* Window::GetRenderer() const {
	return renderer;
}

HWND Window::GetHwnd() const {
	return hwnd;
}

void Window::Run() {
	while (ProcessMessages()) {
		Update();
		Render();
		UpdateInput();
	}
}

void Window::Update() {
}

void Window::Render() const {
}
