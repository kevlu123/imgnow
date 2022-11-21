#include "SDL.h"
#include "app.h"

int main(int argc, char** argv) {
	//const char* _argv[] = {
	//	"[executable name]",
	//	"testdata/1.jpg",
	//	"testdata/2.jpg",
	//	"testdata/3.jpg",
	//};
	//argc = sizeof(_argv) / sizeof(_argv[0]);
	//argv = (char**)_argv;
	
	try {
		App(argc, argv).Run();
	} catch (SDLException& ex) {
		SDL_ShowSimpleMessageBox(0, "Error", ex.what(), nullptr);
		return 2;
	}
	
	return 0;
}
