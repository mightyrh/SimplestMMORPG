#pragma once

#include "stdafx.h"
#include "../../SimplestMMORPG-Server/SimplestMMORPG-Server/protocol.h"

//struct Message
//{
//	Message(uint32_t length, MessageType type, const char* data) : mLength(length),
//																   mType(type)
//	{
//		memcpy(mData, data, length - sizeof(length) - sizeof(type));
//	}
//
//	uint32_t mLength;
//	MessageType mType;
//	char mData[MaxPacketSize - sizeof(mLength) - sizeof(mType)];
//};


class NetworkManager
{
public:
	NetworkManager(const std::string& serverIp);
	~NetworkManager();

	char* pop();
	void send(const char* buffer, int length);
private:
	SOCKET mSocket;
	sockaddr_in mServerAddr;

	std::deque<char*> mMessageQueue;
	std::mutex mQueueMutex;

	bool running;

	std::thread mReceiver;
	void receiverThreadFunc();
	void createReceiverThread();
	void push(char* message);
};