#include "SDL.h"
#include "app.h"
#include "net.h"

static void run(int argc, char** argv) {
	// Load configuration and tcp port for interprocess communication
	Config config;
	uint16_t port = (uint16_t)config.GetOr("port", 29395);
	
	// Initialize networking and automatically cleanup with the destructor
	NetInstance net;

	// Attempt to start the server (this will fail if another instance is already running)
	std::unique_ptr<MessageServer> msgServer;
	try {
		msgServer = std::make_unique<MessageServer>(port);
	} catch (SDLNetException&) {}
	
	if (!msgServer && argc > 1) {
		// Another instance is already running so tell that instance what files to open and exit
		try {
			MessageClient client(port);
			for (int i = 1; i < argc; i++) {
				client.Send(argv[i]);
			}
			return;
		} catch (SDLNetException&) {
			// Failed to send message to other instance so just continue and start the app normally
		}
	}
	
	App(argc, argv, std::move(config), std::move(msgServer)).Run();
}

int main(int argc, char** argv) {
	try {
		run(argc, argv);
		return 0;
	} catch (std::exception& ex) {
		SDL_ShowSimpleMessageBox(0, "Error", ex.what(), nullptr);
		return 1;
	}
}
