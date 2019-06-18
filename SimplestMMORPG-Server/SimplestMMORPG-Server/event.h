#pragma once
#include "simplestMMORPG.h"
#include "protocol.h"


struct Event
{
	ObjectId objectId;
	EventType type;
	std::chrono::high_resolution_clock::time_point startTime;

	constexpr bool operator < (const Event& other) const
	{
		return startTime > other.startTime;
	}
};
