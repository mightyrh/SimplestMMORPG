#pragma once
#include "simplestMMORPG.h"
#include "protocol.h"

struct Object
{
	ClientId clientId = 0;	// 이 값이 0이면 패킷을 보내야 할 대상이 아님.
	WORD objectId;
	SectorId sectorId;
	ObjectType objectType;
	WORD xPos;
	WORD yPos;
	WORD level;
	WORD str;
	WORD dex;
	WORD hp;
	WORD potion;
	WORD kill;
	WORD death;
	DWORD exp;
	SOCKET socket;
	bool initialized;
	std::unordered_set<ObjectId> viewList;
	bool isSleep = true;
};
