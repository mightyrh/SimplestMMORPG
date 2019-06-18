#pragma once

#include "stdafx.h"
#include "../../SimplestMMORPG-Server/SimplestMMORPG-Server/protocol.h"

struct Object
{
	ObjectId objectId;
	ObjectType type;
	WORD xPos;
	WORD yPos;
	WORD hp;
	WORD level;
	DWORD exp;

	Dir dir = Dir::Down;

	Object() {}
};