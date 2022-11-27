#include "SDL.h"
#include "app.h"

int main(int argc, char** argv) {
	const char* _argv[] = {
		"[executable name]",
		"testdata/1.jpg",
		"testdata/2.jpg",
		"testdata/3.jpg",
		"testdata/4.jpg",
		"testdata/5.png",
		"testdata/6.png",
		"testdata/7.png",
		"testdata/8.png",
	};
	argc = sizeof(_argv) / sizeof(_argv[0]);
	argv = (char**)_argv;
	
	try {
		App(argc, argv).Run();
	} catch (std::exception& ex) {
		SDL_ShowSimpleMessageBox(0, "Error", ex.what(), nullptr);
		return 1;
	}
	
	return 0;
}
