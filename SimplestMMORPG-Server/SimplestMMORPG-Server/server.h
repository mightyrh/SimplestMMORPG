#pragma once

#include "simplestMMORPG.h"
#include "protocol.h"
#include "iocp_handler.h"

#include "memory_pool.h"
#include "client.h"
#include "database.h"
#include "object.h"
#include "id_table_def.h"
#include "event.h"


constexpr size_t CLIENT_MEMPOOL_SIZE = 1024;
constexpr size_t WAITING_MEMPOOL_SIZE = 512;
constexpr size_t OVEREX_MEMPOOL_SIZE = 1024;
constexpr size_t OBJECT_MEMPOOL_SIZE = 1024;
constexpr size_t SECTOR_WIDTH = 30;
constexpr size_t SECTOR_HEIGHT = 30;
constexpr size_t MAX_XSECTOR = 10;
constexpr size_t MAX_YSECTOR = 10;
constexpr size_t MAX_SECTOR = MAX_XSECTOR * MAX_YSECTOR;

class Server
{
private:
	IocpHandler mIocpHandler;
	MemoryPool<Client, CLIENT_MEMPOOL_SIZE> mClientPool;
	//MemoryPool<Waiting, WAITING_MEMPOOL_SIZE> mWaitingPool;
	MemoryPool<OverEx, OVEREX_MEMPOOL_SIZE> mOverExMemoryPool;
	MemoryPool<Object, OBJECT_MEMPOOL_SIZE> mObjectPool;
	bool mRunning;

	tbb::concurrent_hash_map<ClientId, size_t, ClientIdHashCompare> mClientIdTable;
	// 접속은 했지만 아직 게임에 들어오지 않은 대기 client들.
	// accept시 먼저 여기에 할당하고 SCLoginOK를 받은 client들만 mClientPool에 메모리를 할당한다.
	// accept할때 임시접속용으로 id를 나눌까? 65535에서 1000정도만?
	//tbb::concurrent_hash_map<ClientId, size_t, WaitingIdHashCompare> mWaitingIdTable;
	tbb::concurrent_hash_map<ObjectId, size_t, ObjectIdHashCompare> mObjectIdTable;
	// Sectors of map
	tbb::concurrent_hash_map<SectorId, std::unordered_set<ObjectId>, SectorHashCompare> mSectorTable;
	
	// Database
	Database mDatabase;
	tbb::concurrent_queue<QueryMessage> mSqlQueryQueue;
	tbb::concurrent_priority_queue<Event> mTimerQueue;
	
	// Threads
	std::thread acceptorThread;
	std::thread timerThread;
	std::thread databaseThread;

	// Thread functions.
	void workerThreadFunc();
	void acceptorThreadFunc();
	void timerThreadFunc();
	void databaseThreadFunc();

	void errorDisplay(const char* msg, int errorNo);
	void shutdownClient(ClientId id);
	void assembleAndProcessPacket(ClientId id, int bytesTransferred);
	void processIo(ClientId id, OverExPtr overExPtr, int bytesTransferred);
	void initPlayer(ClientId id, OverExPtr overExPtr);
	
	// Processing functions
	void processPacket(ClientId id);
	void processEvent(Event& event);
	void addTimer(ObjectId objectId, EventType eventType, std::chrono::steady_clock::time_point startTime);

	void processLogin(ClientId id);
	void processLogout(ClientId id);
	void processMove(ClientId id);
	void processChat(ClientId id);

	void moveObject(SOCKET myClientSocket, ObjectId objectId, Dir dir, SectorId firstSector);

	void recvPost(ClientId id);
	void sendPost(SOCKET socket, ClientId id, const char* buffer, int size);
	void sendPost(ClientId id, const char* buffer, int size);
	bool unsafeSendPost(SOCKET socket, const char* buffer, int size);

	ClientId getAvailableClientId();
	//ClientId getAvailableWaitingId();
	ObjectId getAvailableObjectId();
	void addClient(ClientId id, SOCKET socket);
	//bool isWaitingClient(ClientId id);
	//bool findWaiting(WaitingIdTable::accessor& accessor, ClientId id);
	bool findClient(ClientIdTable::accessor& accessor, ClientId id);
	bool findObject(ObjectIdTable::accessor& accessor, ObjectId id);
	bool findSector(SectorTable::accessor& accessor, SectorId id);

	// Send functions
	void sendLoginOK(ClientId to);
	void sendLoginFail(ClientId to);
	void sendPositionInfo(ClientId to, ClientId id);
	void sendChat(ClientId to, WCHAR* msg);
	void sendStatChange(ClientId id);
	void sendRemoveObject(ClientId to, ObjectId id);
	void sendAddObject(ClientId to, ObjectId id, ObjectType type, WORD xPos, WORD yPos);


	// Sector functions
	SectorId getFirstViewSector(WORD xPos, WORD yPos);
	SectorId getFirstViewSector(SectorId sectorId);
	SectorId getSectorId(WORD xPos, WORD yPos);
	void addObjectToSector(ObjectId id);
	void updateSector(ObjectId objectId, short xPos, short yPos, SectorId sectorId);	// 포인터로 받겠단 말은 미리 lock을 걸어놓으란 말

	// Monster functions
	void initMonsters();

public:
	Server();
	~Server();
};