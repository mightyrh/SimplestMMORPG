#pragma once

#include "simplestMMORPG.h"
#include "protocol.h"
#include "iocp_handler.h"

struct Client
{
	SOCKET socket;
	OverEx overEx;
	char packetBuffer[MAX_PACKET_SIZE];
	MessageSize prevSize;

	WORD objectId;
	WORD dbPK;
	WCHAR idStr[NAME_LEN];
	//SectorId sectorId;
	std::unordered_set<ObjectId> viewList;
};

//struct Waiting
//{
//	SOCKET socket;
//	OverEx overEx;
//	char packetBuffer[MAX_PACKET_SIZE];
//	MessageSize prevSize;
//};