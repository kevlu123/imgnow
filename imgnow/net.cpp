#pragma once
#include "net.h"

NetInstance::NetInstance() {
	SDLNet_Init(); // Ignore error
}

NetInstance::~NetInstance() {
	SDLNet_Quit();
}

const char* SDLNetException::what() const noexcept {
	return SDLNet_GetError();
}

TcpServer::TcpServer(uint16_t port, TcpRecvCallback recvCallback) :
	recvCallback(recvCallback) {
	IPaddress ip{};
	ip.host = SDL_SwapBE32(INADDR_LOOPBACK);
	ip.port = SDL_SwapBE16(port);

	server = SDLNet_TCP_OpenServer(&ip);
	if (!server)
		throw SDLNetException();

	socketSet = SDLNet_AllocSocketSet(MAX_CLIENTS + 1);
	if (!socketSet)
		throw SDLNetException();

	SDLNet_TCP_AddSocket(socketSet, server);

	runThread = std::thread(&TcpServer::Run, this);
}

TcpServer::~TcpServer() {
	running = false;
	runThread.join();
	SDLNet_TCP_Close(server);
	SDLNet_FreeSocketSet(socketSet);
}
	
TcpServer::Client::Client(TCPsocket socket) :
	socket(socket) {
}
	
void TcpServer::Run() {
	while (running) {
		int numReady = SDLNet_CheckSockets(socketSet, 50);
		if (numReady == -1)
			throw SDLNetException();

		if (numReady == 0)
			continue;

		if (SDLNet_SocketReady(server)) {
			TCPsocket clientSocket = SDLNet_TCP_Accept(server);
			if (!clientSocket)
				throw SDLNetException();

			SDLNet_TCP_AddSocket(socketSet, clientSocket);
			clients.push_back(Client(clientSocket));
		}

		for (auto it = clients.begin(); it != clients.end(); ++it) {
			Client& client = *it;
			if (!SDLNet_SocketReady(client.socket))
				continue;
			char buffer[1024];
			int numBytes = SDLNet_TCP_Recv(client.socket, buffer, sizeof(buffer));
			if (numBytes <= 0) {
				SDLNet_TCP_DelSocket(socketSet, it->socket);
				SDLNet_TCP_Close(it->socket);
				it = clients.erase(it);
				if (it == clients.end())
					break;
			} else {
				client.data.insert(client.data.end(), buffer, buffer + numBytes);
				recvCallback(client.data);
			}
		}
	}
}

MessageServer::MessageServer(uint16_t port) :
	server(port, [this](auto& data) { this->OnRecv(data); }) {
}

std::vector<std::string> MessageServer::GetMessages() {
	std::lock_guard<std::mutex> lock(mutex);
	std::vector<std::string> copy = std::move(messages);
	messages.clear();
	return copy;
}
	
void MessageServer::OnRecv(std::vector<uint8_t>& data) {
	while (true) {
		if (data.size() < 4)
			return;

		// Little endian
		uint32_t len = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		if (data.size() < (size_t)len + 4)
			return;

		std::string str((char*)data.data() + 4, len);
		{
			std::lock_guard<std::mutex> lock(mutex);
			messages.push_back(std::move(str));
		}

		data.erase(data.begin(), data.begin() + len + 4);
	}
}
	
MessageClient::MessageClient(uint16_t port) {
	IPaddress ip{};
	ip.host = SDL_SwapBE32(INADDR_LOOPBACK);
	ip.port = SDL_SwapBE16(port);

	socket = SDLNet_TCP_OpenClient(&ip);
	if (!socket)
		throw SDLNetException();
}

MessageClient::~MessageClient() {
	SDLNet_TCP_Close(socket);
}

void MessageClient::Send(std::string_view data) {
	uint32_t len = (uint32_t)data.size();
	std::vector<uint8_t> buffer;
	buffer.push_back(len & 0xFF);
	buffer.push_back((len >> 8) & 0xFF);
	buffer.push_back((len >> 16) & 0xFF);
	buffer.push_back((len >> 24) & 0xFF);
	buffer.insert(buffer.end(), data.begin(), data.end());
	SDLNet_TCP_Send(socket, buffer.data(), (int)buffer.size());
}