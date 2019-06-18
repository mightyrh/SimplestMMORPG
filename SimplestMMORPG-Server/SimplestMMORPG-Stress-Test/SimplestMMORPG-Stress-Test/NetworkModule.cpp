#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <winsock.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <array>
#include <unordered_map>
#include <string>

using namespace std;
using namespace chrono;

extern HWND		hWnd;

const static int MAX_TEST = 20000;
const static int INVALID_ID = -1;
//const static int MAX_PACKET_SIZE = 255;
//const static int MAX_BUFF_SIZE = 255;

#pragma comment (lib, "ws2_32.lib")

#include "..\..\SimplestMMORPG-Server/protocol.h"

HANDLE g_hiocp;
std::mutex printLock;

enum OPTYPE { OP_SEND, OP_RECV, OP_DO_MOVE };

high_resolution_clock::time_point last_connect_time;

struct OverlappedEx {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	char IOCP_buf[255];
	OPTYPE event_type;
	int event_target;
};

struct CLIENT {
	int id;
	int x;
	int y;
	high_resolution_clock::time_point last_move_time;
	bool connect;
	int recvCount = 0;

	SOCKET client_socket;
	OverlappedEx recv_over;
	char packet_buf[MAX_PACKET_SIZE];
	int prev_packet_data;
	int curr_packet_size;
};

array<CLIENT, MAX_CLIENTS> g_clients;
atomic_int num_connections;

vector <thread *> worker_threads;
thread test_thread;

float point_cloud[MAX_CLIENTS * 2];

// 나중에 NPC까지 추가 확장 용
struct ALIEN {
	int id;
	int x, y;
	int visible_count;
};

void error_display(const char *msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"에러" << lpMsgBuf << std::endl;

	MessageBox(hWnd, lpMsgBuf, L"ERROR", 0);
	LocalFree(lpMsgBuf);
	while (true);
}

void DisconnectClient(int ci)
{
	closesocket(g_clients[ci].client_socket);
	g_clients[ci].connect = false;
	cout << "Client [" << ci << "] Disconnected!\n";
}

void ProcessPacket(int ci, char packet[])
{
	MessageType type = static_cast<MessageType>(packet[1]);
	switch (type) {
	case MessageType::SCPositionInfo:
	{
		SCPositionInfo *pos_packet = reinterpret_cast<SCPositionInfo*>(packet);
		if (INVALID_ID == g_clients[ci].id) g_clients[ci].id = ci;
		if (g_clients[ci].id == pos_packet->id) {
			g_clients[ci].x = pos_packet->xPos;
			g_clients[ci].y = pos_packet->yPos;
		}
	} break;
	case MessageType::SCAddObject: break;
	case MessageType::SCRemoveObject: break;
	case MessageType::SCChat: break;
	case MessageType::SCLoginOK:
	{
		g_clients[ci].connect = true;
		SCLoginOK* login_packet = reinterpret_cast<SCLoginOK*>(packet);
		g_clients[ci].id = login_packet->id;
		g_clients[ci].x = login_packet->xPos;
		g_clients[ci].y = login_packet->yPos;
		break;
	}
	default: MessageBox(hWnd, L"Unknown Packet Type", L"ERROR", 0);
	{
		while (true);
	}
	}
}

void Worker_Thread()
{
	while (true) {
		DWORD io_size;
		unsigned long long ci;

		OverlappedEx *over;
		BOOL ret = GetQueuedCompletionStatus(g_hiocp, &io_size, &ci,
											 reinterpret_cast<LPWSAOVERLAPPED *>(&over), INFINITE);


		if (FALSE == ret) {
			int err_no = WSAGetLastError();
			if (64 == err_no) DisconnectClient(ci);
			else error_display("GQCS : ", WSAGetLastError());
		}
		if (0 == io_size) {
			DisconnectClient(ci);
			continue;
		}


		if (OP_RECV == over->event_type) {

		// 패킷 조립
			DWORD rest = io_size;
			char* recvBuffer = g_clients[ci].recv_over.IOCP_buf;
			char* packetBuffer = g_clients[ci].packet_buf;
			int packetSize = 0;
			int prevSize = g_clients[ci].prev_packet_data;

			// 전에 받아놓은 패킷의 사이즈를 읽는다.
			if (prevSize > sizeof(BYTE)) {
				packetSize = static_cast<BYTE>(packetBuffer[0]);
			}
			// 전에 받은 패킷의 사이즈 필드도 완전하지 않을 수 있음.
			else {
				int additionalDataSizeLength = sizeof(MessageSize) - prevSize;

				// 덜 온 데이터 길이도 오다 말았다면 온 만큼 복사해주고 패킷을 만들 수 없으니 false를 리턴.
				if (rest < additionalDataSizeLength) {
					memcpy(&packetBuffer[prevSize], recvBuffer, rest);
					prevSize += rest;
					g_clients[ci].prev_packet_data = prevSize;
					return;
				}

				// mPrevSize부터 sizeof(MessageSize)만큼 까지만 복사해주고 처리한다.
				memcpy(&packetBuffer[prevSize], recvBuffer, additionalDataSizeLength);
				// 데이터 길이를 복사해준 만큼 포인터를 옮겨준다.
				recvBuffer += additionalDataSizeLength;
				prevSize += additionalDataSizeLength;
				rest -= additionalDataSizeLength;
				packetSize = static_cast<MessageSize>(packetBuffer[0]);
			}

			while (rest > 0) {
				if (packetSize == 0) {
					// 패킷 처리하고 남아있는거 처리할 때 데이터 사이즈를 읽을 수 있으면 계속 처리.
					if (rest >= sizeof(MessageSize)) {
						packetSize = static_cast<MessageSize>(recvBuffer[0]);
					}
					// 데이터 사이즈를 읽을 수 없으면 복사해주고 break;
					else {
						memcpy(packetBuffer + prevSize, recvBuffer, rest);
						prevSize = rest;
						break;
					}
				}
				int required = packetSize - prevSize;
				if (rest >= required) {
					// 패킷 생성 가능.
					memcpy(packetBuffer + prevSize, recvBuffer, required);
					
					BYTE type = static_cast<BYTE>(g_clients[ci].packet_buf[1]);
					if (type > 7 || type < 1) {
						printLock.lock();
						std::cout << static_cast<int>(g_clients[ci].packet_buf[0]) << " " << static_cast<int>(g_clients[ci].packet_buf[1]) << std::endl;
						printLock.unlock();
					}
						
				/*	BYTE type = static_cast<BYTE>(g_clients[ci].packet_buf[1]);
					if (type > 7 || type < 1)
						std::cout << "i" << std::endl;*/

					ProcessPacket(ci, g_clients[ci].packet_buf);
					rest -= required;
					recvBuffer += required;
					packetSize = 0;
					prevSize = 0;
				}
				else {
					// 패킷 생성 불가능.
					// 복사해두기.
					memcpy(packetBuffer + prevSize, recvBuffer, rest);
					prevSize += rest;
					rest = 0;
				}
			}

			g_clients[ci].prev_packet_data = prevSize;

			DWORD recv_flag = 0;
			int ret = WSARecv(g_clients[ci].client_socket,
							  &g_clients[ci].recv_over.wsabuf, 1,
							  NULL, &recv_flag, &g_clients[ci].recv_over.over, NULL);
			if (SOCKET_ERROR == ret) {
				int err_no = WSAGetLastError();
				if (err_no != WSA_IO_PENDING) {
					error_display("RECV ERROR", err_no);
				}
			}
		}
		else if (OP_SEND == over->event_type) {
			if (io_size != over->wsabuf.len) {
				std::cout << "Send Incomplete Error!\n";
				closesocket(g_clients[ci].client_socket);
				g_clients[ci].connect = false;
			}
			delete over;
		}
		else if (OP_DO_MOVE == over->event_type) {
			// Not Implemented Yet
			delete over;
		}
		else {
			std::cout << "Unknown GQCS event!\n";
			while (true);
		}
	}

	//	if (FALSE == ret) {
	//		int err_no = WSAGetLastError();
	//		if (64 == err_no) DisconnectClient(ci);
	//		else error_display("GQCS : ", WSAGetLastError());
	//	}
	//	if (0 == io_size) {
	//		DisconnectClient(ci);
	//		continue;
	//	}
	//	if (OP_RECV == over->event_type) {
	//		int tmpRecv = g_clients[ci].recvCount--;
	//		std::cout << "RECV from Client :" << ci;
	//		std::cout << "  IO_SIZE : " << io_size << std::endl;
	//		int tmp = io_size;
	//		unsigned char *buf = g_clients[ci].recv_over.IOCP_buf;
	//		unsigned psize = g_clients[ci].curr_packet_size;
	//		unsigned pr_size = g_clients[ci].prev_packet_data;
	//		int count = 0;
	//		while (io_size > 0) {
	//			count++;
	//			if (0 == psize) psize = buf[0];
	//			if (io_size + pr_size >= psize) {
	//				// 지금 패킷 완성 가능
	//				unsigned char packet[MAX_PACKET_SIZE];
	//				ZeroMemory(packet, MAX_PACKET_SIZE);
	//				memcpy(packet, g_clients[ci].packet_buf, pr_size);
	//				memcpy(packet + pr_size, buf, psize - pr_size);
	//				BYTE type = static_cast<BYTE>(packet[1]);
	//				if (type > 7 || type < 1) {
	//					std::cout << "NANI??? " << type << std::endl;
	//					int error = WSAGetLastError();
	//					std::cout << error << std::endl;
	//				}
	//				ProcessPacket(static_cast<int>(ci), packet);
	//				io_size -= psize - pr_size;
	//				buf += psize - pr_size;
	//				psize = 0; pr_size = 0;
	//			}
	//			else {
	//				memcpy(g_clients[ci].packet_buf + pr_size, buf, io_size);
	//				pr_size += io_size;
	//				io_size = 0;
	//			}
	//		}
	//		g_clients[ci].curr_packet_size = psize;
	//		g_clients[ci].prev_packet_data = pr_size;
	//		DWORD recv_flag = 0;
	//		int ret = WSARecv(g_clients[ci].client_socket,
	//						  &g_clients[ci].recv_over.wsabuf, 1,
	//						  NULL, &recv_flag, &g_clients[ci].recv_over.over, NULL);
	//		g_clients[ci].recvCount++;
	//		if (SOCKET_ERROR == ret) {
	//			int err_no = WSAGetLastError();
	//			if (err_no != WSA_IO_PENDING) {
	//				error_display("RECV ERROR", err_no);
	//			}
	//		}
	//	}
	//	else if (OP_SEND == over->event_type) {
	//		if (io_size != over->wsabuf.len) {
	//			std::cout << "Send Incomplete Error!\n";
	//			closesocket(g_clients[ci].client_socket);
	//			g_clients[ci].connect = false;
	//		}
	//		delete over;
	//	}
	//	else if (OP_DO_MOVE == over->event_type) {
	//		// Not Implemented Yet
	//		delete over;
	//	}
	//	else {
	//		std::cout << "Unknown GQCS event!\n";
	//		while (true);
	//	}
	//}
}

void SendPacket(int cl, void *packet)
{
	int psize = reinterpret_cast<unsigned char *>(packet)[0];
	int ptype = reinterpret_cast<unsigned char *>(packet)[1];
	OverlappedEx *over = new OverlappedEx;
	over->event_type = OP_SEND;
	memcpy(over->IOCP_buf, packet, psize);
	ZeroMemory(&over->over, sizeof(over->over));
	over->wsabuf.buf = reinterpret_cast<CHAR *>(over->IOCP_buf);
	over->wsabuf.len = psize;
	int ret = WSASend(g_clients[cl].client_socket, &over->wsabuf, 1, NULL, 0,
					  &over->over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			error_display("Error in SendPacket:", err_no);
	}
//	std::cout << "Send Packet [" << ptype << "] To Client : " << cl << std::endl;
}

void Adjust_Number_Of_Client()
{
	if (num_connections >= MAX_TEST) return;
	if (high_resolution_clock::now() < last_connect_time + 100ms) return;

	g_clients[num_connections].client_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	int errorCode = NO_ERROR;
	bool optVal = true;	// True : Nagle off, false : Nagle On.
	int returnVal = setsockopt(g_clients[num_connections].client_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&optVal), sizeof(optVal));
	if (returnVal == SOCKET_ERROR) {
		errorCode = WSAGetLastError();
	}
	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(PORT);
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");


	int Result = WSAConnect(g_clients[num_connections].client_socket, (sockaddr *)&ServerAddr, sizeof(ServerAddr), NULL, NULL, NULL, NULL);
	if (0 != Result) {
		error_display("WSAConnect : ", GetLastError());
	}

	g_clients[num_connections].curr_packet_size = 0;
	g_clients[num_connections].prev_packet_data = 0;
	ZeroMemory(&g_clients[num_connections].recv_over, sizeof(g_clients[num_connections].recv_over));
	g_clients[num_connections].recv_over.event_type = OP_RECV;
	g_clients[num_connections].recv_over.wsabuf.buf =
		reinterpret_cast<CHAR *>(g_clients[num_connections].recv_over.IOCP_buf);
	g_clients[num_connections].recv_over.wsabuf.len = sizeof(g_clients[num_connections].recv_over.IOCP_buf);

	DWORD recv_flag = 0;
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_clients[num_connections].client_socket), g_hiocp, num_connections, 0);
	int ret = WSARecv(g_clients[num_connections].client_socket, &g_clients[num_connections].recv_over.wsabuf, 1,
		NULL, &recv_flag, &g_clients[num_connections].recv_over.over, NULL);
	g_clients[num_connections].recvCount++;
	if (SOCKET_ERROR == ret) {
		int err_no = WSAGetLastError();
		if (err_no != WSA_IO_PENDING)
		{
			error_display("RECV ERROR", err_no);
		}
	}

	std::wstring strId = L"TEST" + std::to_wstring(num_connections);
	CSLogin packet(strId);
	SendPacket(num_connections, &packet);

	//g_clients[num_connections].connect = true;
	num_connections++;
}

void Test_Thread()
{
	while (true) {
		Sleep(5);
		Adjust_Number_Of_Client();

		for (int i = 0; i < num_connections; ++i) {
			if (false == g_clients[i].connect) continue;
			if (g_clients[i].last_move_time + 1s > high_resolution_clock::now()) continue;
			g_clients[i].last_move_time = high_resolution_clock::now();
			CSMove my_packet(rand() % 4);
			my_packet.size = sizeof(my_packet);
			/*switch (rand() % 4) {
			case 0: my_packet.type = CS_UP; break;
			case 1: my_packet.type = CS_DOWN; break;
			case 2: my_packet.type = CS_LEFT; break;
			case 3: my_packet.type = CS_RIGHT; break;
			}*/
			SendPacket(i, &my_packet);
		}
	}
}

void InitializeNetwork()
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		g_clients[i].connect = false;
		g_clients[i].id = INVALID_ID;
	}
	num_connections = 0;
	last_connect_time = high_resolution_clock::now();

	WSADATA	wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, 0);

	for (int i = 0; i < 6; ++i)
		worker_threads.push_back(new std::thread{ Worker_Thread });

	test_thread = thread{ Test_Thread };
}

void ShutdownNetwork()
{
	test_thread.join();
	for (auto pth : worker_threads) {
		pth->join();
		delete pth;
	}
}

void Do_Network()
{
	return;
}

void GetPointCloud(int *size, float **points)
{
	for (int i = 0; i < num_connections; ++i) {
		point_cloud[i * 2] = g_clients[i].x;
		point_cloud[i * 2 + 1] = g_clients[i].y;
	}
	*size = num_connections;
	*points = point_cloud;
}

