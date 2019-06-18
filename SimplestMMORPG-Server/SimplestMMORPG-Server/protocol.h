#pragma once

using ClientId = WORD;
using ObjectId = WORD;
using SectorId = WORD;
using MessageSize = BYTE;

inline const char* SERVER_ADDR = "127.0.0.1";
constexpr unsigned short PORT = 54000;

constexpr int MAX_PACKET_SIZE = 255;
constexpr int MAX_BUFFER_SIZE = 255;

constexpr int MAX_CLIENTS = 20000;
constexpr int MAX_WAITING = 1000;
constexpr int MAX_OBJECT = USHRT_MAX;

constexpr int MAP_WIDTH = 300;
constexpr int MAP_HEIGHT = 300;
constexpr int NAME_LEN = 10;
constexpr int VIEW_RANGE = 9;
constexpr int WINDOW_XBLOCKS = 20;
constexpr int WINDOW_YBLOCKS = 20;


enum class MessageType : BYTE
{
	// CS
	CSLogin = 1,
	CSMove = 2,
	CSAttack = 3,
	CSChat = 4,
	CSLogout = 5,

	// SC
	SCLoginOK = 1,
	SCLoginFail = 2,
	SCPositionInfo = 3,
	SCChat = 4,
	SCStatChange = 5,
	SCRemoveObject = 6,
	SCAddObject = 7
};
constexpr size_t MessageHeaderSize = sizeof(MessageSize) + sizeof(MessageType);

enum class ObjectType : BYTE
{
	Player = 1,
	HostileMonster = 2,
	FriendlyMonster = 3,
	Obstacle = 4
};

enum class Dir : BYTE
{
	Down = 0,
	Up = 1,
	Right = 2,
	Left = 3
};

#pragma pack (push, 1)
////////////
//   CS   //
////////////
// 01
struct CSLogin
{
	MessageSize size;
	MessageType type;
	WCHAR idStr[10];	// Zero terminated

	CSLogin(const std::wstring& inStrId)
	{
		size = sizeof(MessageSize) + sizeof(MessageType) + (inStrId.size() + 1) * sizeof(WCHAR);
		type = MessageType::CSLogin;
		wcsncpy_s(idStr, 10, inStrId.c_str(), inStrId.length());
	}
};

// 02
struct CSMove
{
	MessageSize size;
	MessageType type;
	BYTE dir;	// 0:UP, 1:DOWN, 2:LEFT, 3:RIGHT

	CSMove(BYTE inDir) :
		dir(inDir)
	{
		size = sizeof(CSMove);
		type = MessageType::CSMove;
	}
};

// 03
struct CSAttack
{
	MessageSize size;
	MessageType type;
	// 전면 공격
	CSAttack()
	{
		size = sizeof(CSAttack);
		type = MessageType::CSAttack;
	}
};

// 04
struct CSChat
{
	MessageSize size;
	MessageType type;
	WCHAR chat[100];	// Zero terminated

	CSChat(const std::wstring& inChat)
	{
		size = sizeof(MessageSize) + sizeof(MessageType) + (inChat.size() + 1) * sizeof(WCHAR);
		type = MessageType::CSChat;
		wcsncpy_s(chat, 100, inChat.c_str(), inChat.length());
	}
};

// 05
struct CSLogout
{
	MessageSize size;
	MessageType type;

	CSLogout()
	{
		size = sizeof(CSLogout);
		type = MessageType::CSLogout;
	}
};

////////////
//   SC   //
////////////
// 01
struct SCLoginOK
{
	MessageSize size;
	MessageType type;
	WORD id;	// Zero terminated
	WORD xPos;
	WORD yPos;
	WORD hp;
	WORD level;
	DWORD exp;

	SCLoginOK()
	{}

	SCLoginOK(WORD id, WORD xPos, WORD yPos, WORD hp, WORD level, DWORD exp) :
		id(id),
		xPos(xPos),
		yPos(yPos),
		hp(hp),
		level(level),
		exp(exp)
	{
		size = sizeof(SCLoginOK);
		type = MessageType::SCLoginOK;
	}
};

// 02
struct SCLoginFail
{
	MessageSize size;
	MessageType type;

	SCLoginFail()
	{
		size = sizeof(SCLoginFail);
		type = MessageType::SCLoginFail;
	}
};

// 03
struct SCPositionInfo
{
	MessageSize size;
	MessageType type;
	WORD id;
	WORD xPos;
	WORD yPos;

	SCPositionInfo()
	{}

	SCPositionInfo(WORD id, WORD xPos, WORD yPos) :
		id(id),
		xPos(xPos),
		yPos(yPos)
	{
		size = sizeof(SCPositionInfo);
		type = MessageType::SCPositionInfo;
	}
};

// 04
struct SCChat
{
	MessageSize size;
	MessageType type;
	int id;
	WCHAR chat[100];	// Zero terminated

	SCChat()
	{}

	SCChat(const WCHAR* inChat)
	{
		size = sizeof(BYTE) + sizeof(BYTE) + sizeof(inChat);
		type = MessageType::SCChat;
		lstrcat(chat, inChat);
	}
};

// 05
struct SCStatChange
{
	MessageSize size;
	MessageType type;
	int id;
	WORD hp;
	WORD level;
	WORD exp;

	SCStatChange() {}
	SCStatChange(WORD hp, WORD level, WORD exp) :
		hp(hp),
		level(level),
		exp(exp)
	{
		size = sizeof(SCStatChange);
		type = MessageType::SCStatChange;
	}
};

// 06
struct SCRemoveObject
{
	MessageSize size;
	MessageType type;
	WORD id;

	SCRemoveObject() {}
	SCRemoveObject(WORD id) :
		id(id)
	{
		size = sizeof(SCRemoveObject);
		type = MessageType::SCRemoveObject;
	}
};

// 07
struct SCAddObject
{
	MessageSize size;
	MessageType type;
	WORD id;
	BYTE objectType;	// 1:PLAYER, 2:MONSTER1, 3:MONSTER2
	WORD xPos;
	WORD yPos;

	SCAddObject() {}
	SCAddObject(WORD id, ObjectType inObjectType, WORD xPos, WORD yPos) :
		id(id),
		xPos(xPos),
		yPos(yPos)
	{
		size = sizeof(SCAddObject);
		type = MessageType::SCAddObject;
		objectType = static_cast<BYTE>(inObjectType);
	}
};

#pragma pack(pop)

union SCPacket
{
	SCLoginOK loginOK;
	SCLoginFail loginFail;
	SCAddObject addObject;
	SCChat chat;
	SCPositionInfo positionInfo;
	SCRemoveObject removeObject;
	SCStatChange statChange;
};
