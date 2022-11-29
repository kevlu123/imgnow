#include "window.h"
#include "image.h"
#include "SDL_syswm.h"
#include <chrono>

const char* SDLException::what() const noexcept {
	return SDL_GetError();
}

Window::Window(int width, int height) {
	if (SDL_Init(SDL_INIT_VIDEO))
		throw SDLException();

	window = SDL_CreateWindow(
		"",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		width, height,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
	if (!window)
		throw SDLException();

	renderer = SDL_CreateRenderer(
		window,
		-1,
		0);
	if (!renderer)
		throw SDLException();
	
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

Window::~Window() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

bool Window::ProcessMessages() {
	scrollDelta = {};
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
		case SDL_MOUSEBUTTONDOWN:
			mouseStates[ev.button.button - 1] = KeyState::Pressed;
			break;
		case SDL_MOUSEBUTTONUP:
			mouseStates[ev.button.button - 1] = KeyState::Released;
			break;
		case SDL_MOUSEWHEEL:
			scrollDelta.x += ev.wheel.preciseX;
			scrollDelta.y += ev.wheel.preciseY;
			break;
		case SDL_MOUSEMOTION:
			mousePosition.x = ev.motion.x;
			mousePosition.y = ev.motion.y;
			mouseDelta.x += ev.motion.xrel;
			mouseDelta.y += ev.motion.yrel;
			break;
		case SDL_DROPFILE:
			FileDropped(ev.drop.file);
			SDL_free(ev.drop.file);
			break;
		case SDL_WINDOWEVENT:
			switch (ev.window.event) {
			case SDL_WINDOWEVENT_ENTER:
				mouseInWindow = true;
				break;
			case SDL_WINDOWEVENT_LEAVE:
				mouseInWindow = false;
				break;
			case SDL_WINDOWEVENT_RESIZED:
				Resized(ev.window.data1, ev.window.data2);
				break;
			case SDL_WINDOWEVENT_MOVED:
				Moved(ev.window.data1, ev.window.data2);
				break;
			}
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
	
	for (auto& state : mouseStates) {
		if (state == KeyState::Pressed) {
			state = KeyState::Down;
		} else if (state == KeyState::Released) {
			state = KeyState::Up;
		}
	}
	
	mouseDelta = {};
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

bool Window::GetCtrlKeyDown() const {
	return GetKeyDown(SDL_Scancode::SDL_SCANCODE_LCTRL)
		|| GetKeyDown(SDL_Scancode::SDL_SCANCODE_RCTRL);
}

bool Window::GetMouseDown(int button) const {
	return mouseStates[button - 1] == KeyState::Down || mouseStates[button - 1] == KeyState::Pressed;
}

bool Window::GetMousePressed(int button) const {
	return mouseStates[button - 1] == KeyState::Pressed;
}

bool Window::GetMouseReleased(int button) const {
	return mouseStates[button - 1] == KeyState::Released;
}

SDL_FPoint Window::GetScrollDelta() const {
	return scrollDelta;
}

SDL_Point Window::GetMousePosition() const {
	return mousePosition;
}

SDL_Point Window::GetMouseDelta() const {
	return mouseDelta;
}

bool Window::MouseInWindow() const {
	return mouseInWindow;
}

SDL_Point Window::GetClientSize() const {
	SDL_Point p;
	SDL_GetWindowSize(window, &p.x, &p.y);
	return p;
}

SDL_Window* Window::GetWindow() const {
	return window;
}

SDL_Renderer* Window::GetRenderer() const {
	return renderer;
}

float Window::GetDeltaTime() const {
	return deltaTime;
}

void Window::Run() {
	using namespace std::chrono;
	auto lastTime = high_resolution_clock::now();
	while (ProcessMessages()) {
		auto now = high_resolution_clock::now();
		deltaTime = duration_cast<duration<float>>(now - lastTime).count();
		deltaTime = std::min(deltaTime, 0.05f);
		lastTime = now;

		Update();
		UpdateInput();

		SDL_Delay(5);
	}
}

void Window::Update() {
}

void Window::Resized(int width, int height) {
}

void Window::Moved(int x, int y) {
}

void Window::FileDropped(const char* path) {
}
