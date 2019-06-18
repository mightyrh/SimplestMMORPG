#include "simplestMMORPG.h"
#include "iocp_handler.h"

OverEx::OverEx() :
	socket(INVALID_SOCKET),
	ioType(EventType::None)
{
	// Initialize OVERLAPPED struct
	Internal = InternalHigh = 0;
	Offset = OffsetHigh = 0;
	hEvent = nullptr;

	wsaBuf.len = MAX_PACKET_SIZE;
	wsaBuf.buf = buffer;
}

OverEx::OverEx(SOCKET sock, EventType type) :
	socket(sock),
	ioType(type)
{
	// Initialize OVERLAPPED struct
	Internal = InternalHigh = 0;
	Offset = OffsetHigh = 0;
	hEvent = nullptr;

	wsaBuf.len = MAX_PACKET_SIZE;
	wsaBuf.buf = buffer;
}

IocpHandler::IocpHandler() :
	addr(INADDR_LOOPBACK),
	port(PORT),
	listenSocket(INVALID_SOCKET),
	hIocp(nullptr),
	numberOfThreads(0),
	running(false)
{}

IocpHandler::~IocpHandler()
{
	acceptorThread.join();
	for(auto& worker : threadPool) {
		worker->join();
	}
}

int IocpHandler::init(uint32_t addr, uint16_t port, const std::function<void()>& acceptor, const std::function<void()>& worker, int maxNumberOfConcurrentThreads)
{
	this->addr = addr;
	this->port = port;
	numberOfThreads = maxNumberOfConcurrentThreads;

	if (maxNumberOfConcurrentThreads == 0) {
		numberOfThreads = static_cast<int>(std::thread::hardware_concurrency());
	}
	else {
		numberOfThreads = maxNumberOfConcurrentThreads;
	}

	int errorNo = NO_ERROR;
	if ((errorNo = create(numberOfThreads)) != NO_ERROR) {
		return errorNo;
	}

	if((errorNo = create(numberOfThreads)) != NO_ERROR) {
		return errorNo;
	}
	createThreadPool(numberOfThreads);
	if ((errorNo = initWinsock()) != NO_ERROR) {
		return errorNo;
	}

	acceptorThreadFunc = acceptor;
	workerThreadFunc = worker;

	return NO_ERROR;
}

int IocpHandler::init(const char* addr, uint16_t port, const std::function<void()>& acceptor, const std::function<void()>& worker, int maxNumberOfConcurrentThreads)
{
	inet_pton(AF_INET, addr, &this->addr);
	this->addr = ntohl(this->addr);
	this->port = port;
	numberOfThreads = maxNumberOfConcurrentThreads;

	acceptorThreadFunc = acceptor;
	workerThreadFunc = worker;	

	if (maxNumberOfConcurrentThreads == 0) {
		numberOfThreads = static_cast<int>(std::thread::hardware_concurrency());
	}
	else {
		numberOfThreads = maxNumberOfConcurrentThreads;
	}

	int errorNo = NO_ERROR;
	if((errorNo = create(numberOfThreads)) != NO_ERROR) {
		return errorNo;
	}
	
	createThreadPool(numberOfThreads);
	if((errorNo = initWinsock()) != NO_ERROR) {
		return errorNo;
	}


	return NO_ERROR;
}

SOCKET IocpHandler::getListenSocket() const
{
	return listenSocket;
}

HANDLE IocpHandler::getHandle() const
{
	return hIocp;
}

int IocpHandler::getThreadNumber() const
{
	return numberOfThreads;
}

int IocpHandler::recvPost(SOCKET socket, OverExPtr overExPtr)
{
	overExPtr->socket = socket;
	overExPtr->ioType = EventType::Read;

	// Setting OverEx
	DWORD flag = 0;
	DWORD bytesReceived = 0;

	int errorCode = NO_ERROR;
	int returnVal = WSARecv(
		overExPtr->socket,
		&overExPtr->wsaBuf,
		1,
		&bytesReceived,
		&flag,
		static_cast<OVERLAPPED*>(overExPtr),
		nullptr
	);

	if(returnVal == SOCKET_ERROR) {
		errorCode = ::WSAGetLastError();
	}
	if(errorCode == WSA_IO_PENDING) {
		errorCode = NO_ERROR;
	}
	return errorCode;
}

int IocpHandler::sendPost(SOCKET socket, OverExPtr overExPtr, const char* buffer, int length)
{
	overExPtr->socket = socket;
	overExPtr->ioType = EventType::Write;

	// Setting OverEx
	overExPtr->wsaBuf.len = length;
	memcpy(overExPtr->buffer, buffer, length);
	if (length >= 255)
		spdlog::critical("NANI??? {}", length);
	BYTE type = static_cast<BYTE>(overExPtr->buffer[1]);
	if (type > 7 || type < 1)
		spdlog::critical("NANI??? {0}", type);

	DWORD bytesSent = 0;
	DWORD flag = 0;

	int errorCode = NO_ERROR;
	int returnVal = ::WSASend(
		overExPtr->socket,
		&overExPtr->wsaBuf,
		1,
		&bytesSent,
		flag,
		static_cast<OVERLAPPED*>(overExPtr),
		nullptr
	);
	type = static_cast<BYTE>(overExPtr->buffer[1]);
	if (type > 7 || type < 1)
		spdlog::critical("NANI??? {0}", type);

	if(returnVal == SOCKET_ERROR) {
		errorCode = ::WSAGetLastError();
	}
	return errorCode;
}

int IocpHandler::associate(SOCKET socket, int completionKey)
{
	if(hIocp != CreateIoCompletionPort(
		reinterpret_cast<HANDLE>(socket),
		hIocp,
		completionKey,
		0
	)) {
		return WSAGetLastError();
	}
	return NO_ERROR;
}

int IocpHandler::create(int maxNumberOfConcurrentThreads)
{
	hIocp = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		nullptr,
		0,
		maxNumberOfConcurrentThreads
	);

	if(hIocp == nullptr) {
		return WSAGetLastError();
	}
	return NO_ERROR;
}

void IocpHandler::createThreadPool(int maxNumberOfConcurrentThreads)
{
	for(int i=0; i<maxNumberOfConcurrentThreads; ++i) {
		threadPool.emplace_back(std::make_shared<std::thread>(workerThreadFunc));
	}
}

int IocpHandler::initWinsock()
{
	WSAData wsaData;
	if(WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR) {
		return WSAGetLastError();
	}

	listenSocket = WSASocket(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP,
		nullptr,
		0,
		WSA_FLAG_OVERLAPPED
	);

	ZeroMemory(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_addr.S_un.S_addr = htonl(addr);
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(port);

	if (
		bind(
			listenSocket,
			reinterpret_cast<sockaddr*>(&sockAddr),
			sizeof(sockAddr)
		) == SOCKET_ERROR) {
		return WSAGetLastError();
	}

	if(listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		return WSAGetLastError();
	}
	return NO_ERROR;
}
