#include "SDL.h"
#include "app.h"

int main(int argc, char** argv) {
	try {
		App(argc, argv).Run();
		return 0;
	} catch (std::exception& ex) {
		SDL_ShowSimpleMessageBox(0, "Error", ex.what(), nullptr);
		return 1;
	}
}
