#pragma once
#include "SDL_net.h"
#include <stdint.h>
#include <vector>
#include <thread>
#include <functional>
#include <string>
#include <mutex>
#include <exception>

struct NetInstance {
	NetInstance();
	~NetInstance();
	NetInstance(const NetInstance&) = delete;
	NetInstance& operator=(const NetInstance&) = delete;
};

struct SDLNetException : std::exception {
	const char* what() const noexcept override;
};

using TcpRecvCallback = std::function<void(std::vector<uint8_t>&)>;

struct TcpServer {
	TcpServer(uint16_t port, TcpRecvCallback recvCallback);
	~TcpServer();
private:
	struct Client {
		TCPsocket socket;
		std::vector<uint8_t> data;
		explicit Client(TCPsocket socket);
	};
	
	static constexpr size_t MAX_CLIENTS = 16;
	SDLNet_SocketSet socketSet;
	TCPsocket server;
	std::vector<Client> clients;
	std::thread runThread;
	std::atomic_bool running = true;
	TcpRecvCallback recvCallback;
	void Run();
};

struct MessageServer {
	MessageServer(uint16_t port);
	std::vector<std::string> GetMessages();
private:
	TcpServer server;
	std::vector<std::string> messages;
	std::mutex mutex;
	void OnRecv(std::vector<uint8_t>& data);
};

struct MessageClient {
	MessageClient(uint16_t port);
	~MessageClient();
	void Send(std::string_view data);
private:
	TCPsocket socket;
};