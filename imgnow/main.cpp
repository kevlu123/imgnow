#include "SDL.h"
#include "app.h"

int main(int argc, char** argv) {
	//if (argc <= 1) {
	//	// Maybe add file picker instead later if I feel like it
	//	SDL_ShowSimpleMessageBox(0, "Error", "No files specified", NULL);
	//	return 1;
	//}
	
	try {
		App(argc, argv).Run();
	} catch (SDLException& ex) {
		SDL_ShowSimpleMessageBox(0, "Error", ex.what(), NULL);
		return 2;
	}
	
	return 0;
}
