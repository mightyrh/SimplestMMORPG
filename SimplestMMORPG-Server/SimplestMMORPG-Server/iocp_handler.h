#pragma once

#include "simplestMMORPG.h"
#include "protocol.h"


enum class EventType : int
{
	None = 0,
	Read = 1,
	Write = 2,
	NpcMove = 3,
	Heal = 4,
	NpcDetectPlayer = 5
};

struct OverEx : public OVERLAPPED
{
	WSABUF wsaBuf;
	SOCKET socket;
	EventType ioType;
	char buffer[MAX_PACKET_SIZE];

	OverEx();
	OverEx(SOCKET sock, EventType type);
};
using OverExPtr = OverEx * ;


class IocpHandler
{
private:
	uint32_t addr;
	uint16_t port;
	SOCKET listenSocket;
	sockaddr_in sockAddr;

	HANDLE hIocp;
	int numberOfThreads;
	bool running;

	std::function<void()> acceptorThreadFunc;
	std::function<void()> workerThreadFunc;
	std::thread acceptorThread;
	std::vector<std::shared_ptr<std::thread>> threadPool;

public:
	IocpHandler();
	~IocpHandler();

	int init(uint32_t addr, uint16_t port, const std::function<void()>& acceptor, const std::function<void()>& worker, int maxNumberOfConcurrentThreads = 0);
	int init(const char* addr, uint16_t port, const std::function<void()>& acceptor, const std::function<void()>& worker, int maxNumberOfConcurrentThreads = 0);

	SOCKET getListenSocket() const;
	HANDLE getHandle() const;
	int getThreadNumber() const;

	int recvPost(SOCKET socket, OverExPtr overExPtr);
	int sendPost(SOCKET socket, OverExPtr overExPtr, const char* buffer, int length);
	int associate(SOCKET socket, int completionKey);

private:
	int create(int maxNumberOfConcurrentThreads);
	void createThreadPool(int maxNumberOfConcurrentThreads);
	int initWinsock();

};