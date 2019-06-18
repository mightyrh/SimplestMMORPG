#pragma once

#include "simplestMMORPG.h"
#include "protocol.h"

struct ClientIdHashCompare
{
	static size_t hash(const ClientId& x)
	{
		size_t h = x;
		return h;
	}
	static bool equal(const ClientId& x, const ClientId& y)
	{
		return x == y;
	}
};

struct WaitingIdHashCompare
{
	static size_t hash(const ClientId& x)
	{
		size_t h = x;
		return h;
	}
	static bool equal(const ClientId& x, const ClientId& y)
	{
		return x == y;
	}
};

struct ObjectIdHashCompare
{
	static size_t hash(const ObjectId& x)
	{
		size_t h = x;
		return h;
	}
	static bool equal(const ObjectId& x, const ObjectId& y)
	{
		return x == y;
	}
};

struct SectorHashCompare
{
	static size_t hash(const SectorId& x)
	{
		size_t h = x;
		return h;
	}
	static bool equal(const SectorId& x, const SectorId& y)
	{
		return x == y;
	}
};

typedef tbb::concurrent_hash_map<ClientId, size_t, ClientIdHashCompare> ClientIdTable;
//typedef tbb::concurrent_hash_map<ClientId, size_t, WaitingIdHashCompare> WaitingIdTable;
typedef tbb::concurrent_hash_map<ObjectId, size_t, ObjectIdHashCompare> ObjectIdTable;
typedef tbb::concurrent_hash_map<SectorId, std::unordered_set<ObjectId>, SectorHashCompare> SectorTable;

