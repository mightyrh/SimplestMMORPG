#include "stdafx.h"
#include "network_manager.h"
#include <string>

NetworkManager::NetworkManager(const std::string& serverIp) : mSocket(INVALID_SOCKET),
															  running(false)
{
	WSAData wsaData;
	if(WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
		std::cout << "ERROR " << WSAGetLastError() << ": WSAStartup() failed." << std::endl;
		return;
	}

	mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	mServerAddr.sin_family = AF_INET;
	inet_pton(AF_INET, serverIp.c_str(), &mServerAddr.sin_addr.S_un.S_addr);
	mServerAddr.sin_port = htons(PORT);

	if(::connect(mSocket, reinterpret_cast<sockaddr*>(&mServerAddr), sizeof(mServerAddr)) == SOCKET_ERROR) {
		std::cout << "ERROR " << WSAGetLastError() << ": connect() failed." << std::endl;
		return;
	}

	std::cout << "Connected." << std::endl;

	running = true;
	createReceiverThread();


	std::cout << "Enter your Id. (ex: player1, player2...)" << std::endl;
	// id를 입력하고 전송한다.
	std::wstring id;
	std::getline(std::wcin, id);
	CSLogin message(id.c_str());

	send(reinterpret_cast<const char*>(&message), message.size);	
}

NetworkManager::~NetworkManager()
{
	running = false;
	mReceiver.join();
	closesocket(mSocket);
	WSACleanup();
}

char* NetworkManager::pop()
{
	{
		std::scoped_lock<std::mutex> lock(mQueueMutex);
		if (!mMessageQueue.empty()) {
			char* data = mMessageQueue.front();
			mMessageQueue.pop_front();
			return data;
		}
		return nullptr;
	}
}

void NetworkManager::send(const char* buffer, int length)
{
	int32_t bytesSent = ::send(mSocket, buffer, length, 0);
	if(bytesSent == SOCKET_ERROR) {
		std::cout << "ERROR " << WSAGetLastError() << ": send() failed." << std::endl;
	}
	std::cout << "Packet sent." << std::endl;
}

void NetworkManager::push(char* message)
{
	{
		std::scoped_lock<std::mutex> lock(mQueueMutex);
		mMessageQueue.push_back(message);
	}
}

void NetworkManager::receiverThreadFunc()
{
	int32_t bytesReceived = 0;
	MessageType type;
	char buffer[256];

	while (running) {
		bytesReceived = ::recv(mSocket, reinterpret_cast<char*>(&buffer[0]), sizeof(MessageSize), 0);
		if (bytesReceived == SOCKET_ERROR) {
			std::cout << "ERROR " << WSAGetLastError() << ": recv() failed." << std::endl;
			return;
		}

		bytesReceived = ::recv(mSocket, reinterpret_cast<char*>(&buffer[sizeof(MessageSize)]), static_cast<MessageSize>(buffer[0]) - sizeof(MessageSize), 0);
		if (bytesReceived == SOCKET_ERROR) {
			std::cout << "ERROR " << WSAGetLastError() << ": recv() failed." << std::endl;
			return;
		}

		char* packet = new char[bytesReceived + sizeof(MessageSize)];
		memcpy_s(packet, sizeof(MessageSize) + bytesReceived, buffer, sizeof(MessageSize) + bytesReceived);
		

		push(packet);
	}
}

void NetworkManager::createReceiverThread()
{
	mReceiver = std::thread{ [this]() {this->receiverThreadFunc(); } };
}

