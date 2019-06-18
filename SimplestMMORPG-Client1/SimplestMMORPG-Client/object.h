#pragma once

#include "common.h"

struct Object
{
	WORD id;
};

struct Player
{
	WORD id;
	WCHAR idStr[10];
	WORD level;
	WORD xPos;
	WORD yPos;
	WORD str;
	WORD dex;
	WORD hp;
	WORD potion;
	WORD kill;
	WORD death;
	DWORD exp;
};
