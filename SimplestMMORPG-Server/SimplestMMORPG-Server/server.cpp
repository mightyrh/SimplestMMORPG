#include "simplestMMORPG.h"
#include "server.h"


void Server::workerThreadFunc()
{
	DWORD bytesTransferred = 0;
	ClientId completionKey;
	bool success = false;
	OverExPtr overExPtr = nullptr;

	while(true) {
		success = GetQueuedCompletionStatus(
			mIocpHandler.getHandle(),
			&bytesTransferred,
			reinterpret_cast<PULONG_PTR>(&completionKey),
			reinterpret_cast<LPOVERLAPPED*>(&overExPtr),
			INFINITE);

		// Error check.
		if(!success) {
			int errorCode = WSAGetLastError();
			if (errorCode != 64)
				errorDisplay("GQCS failed.", errorCode);

			if(completionKey != 0) {
				shutdownClient(completionKey);
			}
			else {
				errorDisplay("GQCS failed and completion key was 0.", errorCode);
			}

			// overExPtr이 nullptr이 아니라면 메모리를 해제해준다.
			if(overExPtr != nullptr) {
				mOverExMemoryPool.dealloc(overExPtr);
			}
			continue;
		}

		// 서버 종료 시 메모리를 해제하고 PQCS를 이용해 worker thread를 종료한다.
		if(completionKey == 0) {
			spdlog::info("Worker thread killed. Ignore this when shutting down the server.");

			// 메모리 해제.
			if(overExPtr != nullptr) {
				mOverExMemoryPool.dealloc(overExPtr);
			}
			break;
		}

		// IoType이 IoType::None이거나 bytesTransferred가 0일 경우 client의 연결을 끊는다.
		if(bytesTransferred == 0 || overExPtr->ioType == EventType::None) {
			shutdownClient(completionKey);
		}

		// Valid한 Io가 넘어왔을 경우 처리.
		else {
			processIo(completionKey, overExPtr, bytesTransferred);
		}

		// 모든 처리 후 메모리가 해제되었는지 check.
		if(overExPtr != nullptr && overExPtr->ioType != EventType::Read) {
			mOverExMemoryPool.dealloc(overExPtr);
		}
	}
}

void Server::acceptorThreadFunc()
{
	int errorCode = 0;

	while(mRunning) {
		sockaddr_in clientAddr;
		char cstrAddr[INET_ADDRSTRLEN];
		int addrSize = sizeof(clientAddr);
		SOCKET clientSocket = INVALID_SOCKET;

		clientSocket = WSAAccept(
			mIocpHandler.getListenSocket(),
			reinterpret_cast<sockaddr*>(&clientAddr),
			&addrSize,
			nullptr,
			0
		);

		// Error check.
		if(clientSocket == INVALID_SOCKET) {
			if (mRunning) {
				errorCode = WSAGetLastError();
				errorDisplay("WSAAccept failed, socket was INVALID_SOCKET.", errorCode);
				continue;
			}
			else {
				spdlog::info("Terminating acceptor thread.");
				break;
			}
		}

		// Nagle 끄기.
		bool optVal = true;	// True : Nagle off, false : Nagle On.
		int returnVal = setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&optVal), sizeof(optVal));
		if(returnVal == SOCKET_ERROR) {
			errorCode = WSAGetLastError();
			errorDisplay("Failed to turn nagle off.", errorCode);
		}

		//// Waiting용 Id 부여.
		//std::string_view clientName = cstrAddr;
		//ClientId id = getAvailableWaitingId();
		//if(id == 0) {
		//	spdlog::warn("Waiting is full. Can't take no more waiting. (Server::acceptorThreadFunc())");
		//	return;
		//}
		ClientId id = getAvailableClientId();
		if (id == 0) {
			spdlog::warn("Client is full. Can't take no more clients. (Server::acceptorThreadFunc())");
			return;
		}

		// Associate with iocp.
		mIocpHandler.associate(clientSocket, id);
		
		//// Waiting에 추가시켜준다.
		//WaitingIdTable::accessor accessor;
		//Waiting* waiting;
		//size_t index = mWaitingPool.alloc(waiting);
		//waiting->socket = clientSocket;

		//if(mWaitingIdTable.insert(accessor, id)) {
		//	accessor->second = index;
		//}
		//accessor.release();

		// Client 할당
		ClientIdTable::accessor accessor;
		Client* client;
		size_t index = mClientPool.alloc(client);
		client->socket = clientSocket;

		if (mClientIdTable.insert(accessor, id)) {
			accessor->second = index;
		}
		accessor.release();

		spdlog::trace("New client{0} accepted.", id);

		recvPost(id);
	}
}

//TODO: id를 인자로 받을 필요가 있나? DB에서 읽어와서 적용시킬건데?
void Server::addClient(ClientId id, SOCKET socket)
{
	////TODO: DB 접속해서 id, stat등 읽어와서 적용시키기.
	//ClientId clientId;
	//WORD xPos;
	//WORD yPos;
	//WORD hp;
	//WORD level;
	//WORD exp;

	//
	//ClientIdTable::accessor accessor;
	//Client* clientPtr;

	//size_t index = mClientPool.alloc(clientPtr);
	//clientPtr->socket = socket;

	//if(mClientIdTable.insert(accessor, id)) {
	//	accessor->second = index;
	//}

	//
	//SCLoginOK data(accessor->first, xPos, yPos, hp, level, exp);
	//data.id = accessor->first;
	////TODO: 패킷 보내기.

	//accessor.release();
	//sendPost(data.id, reinterpret_cast<const char*>(&data));
}

// 해당 클라이언트가 login한 클라이언트인지 waiting 클라이언트인지 확인한다.
//bool Server::isWaitingClient(ClientId id)
//{
//	if (id >= MAX_CLIENTS - MAX_WAITING) {
//		return true;
//	}
//	return false;
//}

//bool Server::findWaiting(WaitingIdTable::accessor & accessor, ClientId id)
//{
//	if (!mWaitingIdTable.find(accessor, id)) {
//		spdlog::warn("Can't find waiting in WaitingIdTable.");
//		return false;
//	}
//	return true;
//}

bool Server::findClient(ClientIdTable::accessor & accessor, ClientId id)
{
	if (!mClientIdTable.find(accessor, id)) {
		spdlog::warn("Can't find client in ClientIdTable.");
		return false;
	}
	return true;
}

bool Server::findObject(ObjectIdTable::accessor & accessor, ObjectId id)
{
	if (!mObjectIdTable.find(accessor, id)) {
		spdlog::warn("Can't find object in ClientIdTable.");
		return false;
	}
	return true;
}

bool Server::findSector(SectorTable::accessor & accessor, SectorId id)
{
	if (!mSectorTable.find(accessor, id)) {
		spdlog::warn("Can't find sector in SectorTable. ID: {}", id);
		return false;
	}
	return true;
}

void Server::sendLoginOK(ClientId to)
{
	ClientIdTable::accessor clientAccessor;
	findClient(clientAccessor, to);
	Client* client = mClientPool.unsafeAt(clientAccessor->second);
	ObjectId objectId = client->objectId;
	clientAccessor.release();

	ObjectIdTable::accessor objectAccessor;
	findObject(objectAccessor, objectId);
	Object* object = mObjectPool.unsafeAt(objectAccessor->second);	

	SCLoginOK packet(object->objectId, object->xPos, object->yPos, object->hp, object->level, object->exp);
	objectAccessor.release();

	sendPost(to, reinterpret_cast<const char*>(&packet), packet.size);
}

void Server::sendLoginFail(ClientId to)
{
	SCLoginFail packet;
	sendPost(to, reinterpret_cast<const char*>(&packet), packet.size);
}

void Server::sendPositionInfo(ClientId to, ObjectId id)
{
	ObjectIdTable::accessor objectAccessor;
	findObject(objectAccessor, id);
	Object* object = mObjectPool.unsafeAt(objectAccessor->second);

	SCPositionInfo packet(id, object->xPos, object->yPos);
	objectAccessor.release();

	sendPost(to, reinterpret_cast<const char*>(&packet), packet.size);
}

void Server::sendChat(ClientId to, WCHAR* msg)
{
	SCChat packet(msg);
	sendPost(to, reinterpret_cast<const char*>(&packet), packet.size);
}

void Server::sendStatChange(ClientId to)
{
	ClientIdTable::accessor clientAccessor;
	findClient(clientAccessor, to);
	Client* client = mClientPool.unsafeAt(clientAccessor->second);
	ObjectId objectId = client->objectId;
	clientAccessor.release();

	ObjectIdTable::accessor objectAccessor;
	findObject(objectAccessor, objectId);
	Object* object = mObjectPool.unsafeAt(objectAccessor->second);

	SCStatChange packet(object->hp, object->level, object->exp);
	objectAccessor.release();
	sendPost(to, reinterpret_cast<const char*>(&packet), packet.size);
}

void Server::sendRemoveObject(ClientId to, ObjectId id)
{
	SCRemoveObject packet(id);
	sendPost(to, reinterpret_cast<const char*>(&packet), packet.size);
}

void Server::sendAddObject(ClientId to, ObjectId id, ObjectType type, WORD xPos, WORD yPos)
{
	SCAddObject packet(id, type, xPos, yPos);
	sendPost(to, reinterpret_cast<const char*>(&packet), packet.size);
}

void Server::timerThreadFunc()
{
	Event event;
	while (true) {
		std::this_thread::sleep_for(5ms);
		while (mTimerQueue.try_pop(event)) {
			while (true) {
				if (event.startTime > std::chrono::high_resolution_clock::now()) {
					std::this_thread::sleep_for(5ms);
					continue;
				}
				OverEx* overEx = mOverExMemoryPool.alloc();
				overEx->ioType = event.type;
				PostQueuedCompletionStatus(mIocpHandler.getHandle(), 1, event.objectId, reinterpret_cast<LPOVERLAPPED>(overEx));
				break;
			}
		}
	}
}

void Server::databaseThreadFunc()
{
	QueryMessage queryMessage;
	while (mRunning) {
		std::this_thread::sleep_for(10ms);
		while(mSqlQueryQueue.try_pop(queryMessage)) {
			switch (queryMessage.type) {
			case QueryType::Login:
			{
				ClientId retId = -1;
				int retLevel = -1;
				int retXPos = INT_MIN;
				int retYPos = INT_MIN;
				int retSTR = INT_MIN;
				int retDEX = INT_MIN;
				int retHp = INT_MIN;
				int retPotion = INT_MIN;
				int retKill = INT_MIN;
				int retDeath = INT_MIN;
				int retExp = INT_MIN;


				bool success = mDatabase.login(queryMessage.query, retId, retLevel, retXPos, retYPos, retSTR, retDEX, retHp, retPotion, retKill, retDeath, retExp);
				if (success) {
					// object pool에 할당
					Object* object;
					size_t objectIndex = mObjectPool.alloc(object);
					object->initialized = false;

					ObjectId objectId = getAvailableObjectId();
					std::random_device rd;
					std::default_random_engine dre(rd());
					std::uniform_real_distribution<> xUid(0, MAP_WIDTH - 1);
					std::uniform_real_distribution<> yUid(0, MAP_HEIGHT - 1);

					// Object 초기화
					object->objectType = ObjectType::Player;
					object->objectId = objectId;
					object->level = retLevel;
					//TODO: 아이디 앞에 "TEST"가 있는 애들만 랜덤으로 배치해줘야 함.
					object->xPos = xUid(dre);
					object->yPos = yUid(dre);
					//object->xPos = 4;
					//object->yPos = 4;
					object->str = retSTR;
					object->dex = retDEX;
					object->hp = retHp;
					object->potion = retPotion;
					object->kill = retKill;
					object->death = retDeath;
					object->exp = retExp;
					object->clientId = queryMessage.clientId;

					ClientIdTable::accessor clientAccessor;
					if (!findClient(clientAccessor, queryMessage.clientId)) {
						clientAccessor.release();
						return;
					}
					Client* client = mClientPool.unsafeAt(clientAccessor->second);

					object->socket = client->socket;
					SOCKET clientSocket = client->socket;
					// Client 초기화
					client->dbPK = retId;
					client->objectId = objectId;
					
					//client->socket = clientSocket;
					//mIocpHandler.associate(clientSocket, newId);
					clientAccessor.release();


					///////////////////////////////////////////////////////////
					// Sector에 포함시키기
					SectorId sectorId = getSectorId(object->xPos, object->yPos);
					SectorId firstSectorId = getFirstViewSector(sectorId);
					object->sectorId = sectorId;
					
					ObjectIdTable::accessor objectAccessor;
					mObjectIdTable.insert(objectAccessor, objectId);
					objectAccessor->second = objectIndex;
					object->initialized = true;
					SCLoginOK packet(object->objectId, object->xPos, object->yPos, object->hp, object->level, object->exp);
					SOCKET objectSocket = object->socket;
					objectAccessor.release();

					SectorTable::accessor sectorAccessor;
					// view list 초기화해줘야 함.
					//for (int i = 0; i < 4; ++i) {
					//	int offset = (i / 2) * MAX_XSECTOR + (i % 2);
					//	if (!findSector(sectorAccessor, firstSectorId + offset)) {
					//		sectorAccessor.release();
					//		break;
					//	}

					//	std::unordered_set<ObjectId> objects = sectorAccessor->second;
					//	sectorAccessor.release();

					//	for (auto& otherObjectId : objects) {
					//		if (otherObjectId == objectId)
					//			continue;
					//		ObjectIdTable::accessor otherObjectAccessor;
					//		if (!findObject(otherObjectAccessor, otherObjectId)) {
					//			otherObjectAccessor.release();
					//			if (!findSector(sectorAccessor, firstSectorId + offset)) {
					//				sectorAccessor.release();
					//				continue;
					//			}
					//			sectorAccessor->second.erase(otherObjectId);
					//			sectorAccessor.release();
					//			continue;
					//		}
					//		Object* otherObject = mObjectPool.unsafeAt(otherObjectAccessor->second);
					//		SOCKET otherClientSocket = otherObject->socket;
					//		ClientId otherClientId = otherObject->clientId;
					//		ObjectId otherObjectId = otherObject->objectId;
					//		short otherXPos = otherObject->xPos;
					//		short otherYPos = otherObject->yPos;
					//		ObjectType otherType = otherObject->objectType;
					//		otherObjectAccessor.release();

					//		if (std::abs(otherXPos - object->xPos) <= VIEW_RANGE &&
					//			std::abs(otherYPos - object->yPos) <= VIEW_RANGE) {
					//			client->viewList.insert(otherObjectId);

					//			ObjectIdTable::accessor objectAccessor;
					//			// view list에 추가해준다.
					//			// 내 오브젝트 view list에 추가
					//			if (object->viewList.count(otherObjectId) == 0) {
					//				object->viewList.insert(otherObjectId);
					//				if (otherType != ObjectType::Obstacle && otherType != ObjectType::Player) {
					//					if (!findObject(objectAccessor, otherObjectId)) {
					//						objectAccessor.release();
					//						continue;
					//					}
					//					otherObject = mObjectPool.unsafeAt(objectAccessor->second);
					//					if (otherObject->isSleep) {
					//						otherObject->isSleep = false;
					//						addTimer(otherObjectId, EventType::NpcMove, std::chrono::high_resolution_clock::now());
					//						spdlog::info("NpcMove added to timer.");
					//					}
					//					objectAccessor.release();
					//				}
					//			}


					//			// 상대 오브젝트 view list에 추가
					//			if (!findObject(objectAccessor, otherObjectId)) {
					//				objectAccessor.release();
					//			}
					//			otherObject = mObjectPool.unsafeAt(objectAccessor->second);
					//			if (otherObject->viewList.count(objectId) == 0)
					//				otherObject->viewList.insert(objectId);
					//			objectAccessor.release();

					//			// 상대에게 자신을 추가하라고 보냄
					//			if (otherType == ObjectType::Player) {
					//				SCAddObject packet(objectId, ObjectType::Player, object->xPos, object->yPos);
					//				//sendPost(otherClientSocket, otherClientId, reinterpret_cast<const char*>(&packet), packet.size);
					//				sendPost(otherClientId, reinterpret_cast<const char*>(&packet), packet.size);
					//				ClientIdTable::accessor otherClientAccessor;
					//				if (findClient(otherClientAccessor, otherClientId)) {
					//					Client* otherClient = mClientPool.unsafeAt(otherClientAccessor->second);
					//					otherClient->viewList.insert(objectId);
					//					int i = 1;
					//					i += 10;
					//				}
					//				otherClientAccessor.release();
					//			}
					//			// 새로 들어온 클라이언트에겐 view list에 있으면 다 추가하라고 보낸다.
					//			SCAddObject packet(otherObjectId, otherType, otherXPos, otherYPos);
					//			//sendPost(clientSocket, queryMessage.clientId, reinterpret_cast<const char*>(&packet), packet.size);
					//			sendPost(queryMessage.clientId, reinterpret_cast<const char*>(&packet), packet.size);
					//		}
					//	}

					//}
					/////////////////////////////////////////////////////////////////////




					// Sector에 포함시키기
					if (!findSector(sectorAccessor, sectorId)) {
						sectorAccessor.release();
					}
					sectorAccessor->second.insert(objectId);
					sectorAccessor.release();

					sendPost(objectSocket, queryMessage.clientId, reinterpret_cast<const char*>(&packet), packet.size);
					//sendPost(queryMessage.clientId, reinterpret_cast<const char*>(&packet), packet.size);
					//sendLoginOK(queryMessage.clientId);
				}
				else {
					// login fail
					spdlog::critical("Login Fail!!!!!!");
					sendLoginFail(queryMessage.clientId);
					shutdownClient(queryMessage.clientId);
				}
				break;
			}

			case QueryType::Logout:
				mDatabase.logout(queryMessage.query);
				break;

			default:
				spdlog::error("Unknown query.");
				break;
			}
		}
	}
}

void Server::errorDisplay(const char* msg, int errorNo)
{
	WCHAR* lpMsgBuf = nullptr;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		errorNo,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		reinterpret_cast<LPWSTR>(&lpMsgBuf),
		0,
		nullptr
	);
	std::string sMsg(msg);
	std::wstring wMsg;
	wMsg.assign(sMsg.begin(), sMsg.end());

	spdlog::critical(L"{0} Error[{1}]: {2}", wMsg, errorNo, lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void Server::shutdownClient(ClientId id)
{
	//WaitingIdTable::accessor waitingAccessor;
	ClientIdTable::accessor clientAccessor;
	//Waiting* waiting = nullptr;
	Client* client = nullptr;

	//if (isWaitingClient(id)) {
	//	if (!findWaiting(waitingAccessor, id)) {
	//		return;
	//	}
	//	waiting = mWaitingPool.unsafeAt(waitingAccessor->second);
	//}
	//else {
	if (!findClient(clientAccessor, id)) {
		return;
	}
	client = mClientPool.unsafeAt(clientAccessor->second);
	ObjectId objectId = client->objectId;
	int dbPK = client->dbPK;
	SOCKET clientSocket = client->socket;

	mClientIdTable.erase(clientAccessor);
	clientAccessor.release();

	//}

	//// 클라이언트 메모리 해제
	//if (waiting != nullptr) {
	//	shutdown(waiting->socket, SD_SEND);
	//	closesocket(waiting->socket);
	//	mWaitingIdTable.erase(waitingAccessor->first);
	//	mWaitingPool.dealloc(waiting);
	//}

	// 클라이언트 메모리 해제, DB에 저장
	if (client != nullptr) {
		ObjectIdTable::accessor objectAccessor;
		Object* object = nullptr;
		ObjectId objectId = client->objectId;
		if (findObject(objectAccessor, objectId)) {
			object = mObjectPool.unsafeAt(objectAccessor->second);
			objectId = object->objectId;
		}

		QueryMessage query;
		query.clientId = id;
		query.type = QueryType::Logout;
		query.query = mDatabase.sqlBuildQueryLogout(
			dbPK,
			object->level,
			object->xPos,
			object->yPos,
			object->str,
			object->dex,
			object->hp,
			object->potion,
			object->kill,
			object->death,
			object->exp
		);
		mSqlQueryQueue.push(query);

		shutdown(clientSocket, SD_SEND);
		closesocket(clientSocket);
		mObjectIdTable.erase(objectAccessor);
		objectAccessor.release();
		mObjectPool.dealloc(object);

	}
	mClientPool.dealloc(client);
	//TODO: sector에서 빼주기.

	spdlog::info("Client{0} shutdown.", id);
}

// Id에 따라서 어떤 IdTable을 사용할지 나눠야 함.
// waitingTable에 저장될 데이터에도 recvBuffer, packetBuffer, prevSize가 필요함.
void Server::assembleAndProcessPacket(ClientId id, int bytesTransferred)
{
	bool goodToProcess = false;

	ClientIdTable::accessor clientAccessor;
	//WaitingIdTable::accessor waitingAccessor;
	Client* client = nullptr;
	//Waiting* waiting = nullptr;

	int rest = bytesTransferred;
	char* recvBuffer = nullptr;
	char* packetBuffer = nullptr;
	int packetSize = 0;
	int prevSize = 0;

	// Waiting 클라이언트인가?
	//if(isWaitingClient(id)) {
	//	if(!mWaitingIdTable.find(waitingAccessor, id)) {
	//		spdlog::warn("Can't find client in WaitingIdTable. (Server::assembleAndProcessPacket())");
	//		return id;
	//	}

	//	waiting = mWaitingPool.unsafeAt(waitingAccessor->second);
	//	recvBuffer = waiting->overEx.buffer;
	//	packetBuffer = waiting->packetBuffer;
	//	prevSize = waiting->prevSize;
	//}
	//// 접속된 클라이언트인가?
	//else {
		if (!mClientIdTable.find(clientAccessor, id)) {
			spdlog::warn("Can't find client in ClientIdTable. (Server::assembleAndProcessPacket())");
			return;
		}

		client = mClientPool.unsafeAt(clientAccessor->second);
		recvBuffer = client->overEx.buffer;
		packetBuffer = client->packetBuffer;
		prevSize = client->prevSize;
		clientAccessor.release();
	//}


	// 전에 받아놓은 패킷의 사이즈를 읽는다.
	if(prevSize > sizeof(BYTE)) {
		packetSize = static_cast<BYTE>(packetBuffer[0]);
	}
	// 전에 받은 패킷의 사이즈 필드도 완전하지 않을 수 있음.
	else {
		int additionalDataSizeLength = sizeof(MessageSize) - prevSize;

		// 덜 온 데이터 길이도 오다 말았다면 온 만큼 복사해주고 패킷을 만들 수 없으니 false를 리턴.
		if(rest < additionalDataSizeLength) {
			memcpy(&packetBuffer[prevSize], recvBuffer, rest);
			prevSize += rest;
			if (findClient(clientAccessor, id))
				mClientPool.unsafeAt(clientAccessor->second)->prevSize = prevSize;
			//else
			//	mWaitingPool.unsafeAt(waitingAccessor->second)->prevSize = prevSize;
			clientAccessor.release();
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

	while(rest > 0) {
		if(packetSize == 0) {
			// 패킷 처리하고 남아있는거 처리할 때 데이터 사이즈를 읽을 수 있으면 계속 처리.
			if(rest >= sizeof(MessageSize)) {
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
		if(rest >= required) {
			// 패킷 생성 가능.
			memcpy(packetBuffer + prevSize, recvBuffer, required);
			goodToProcess = true;
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

	if (client != nullptr) {
		client->prevSize = prevSize;
		clientAccessor.release();
	}
	//else {
	//	waiting->prevSize = prevSize;
	//	waitingAccessor.release();
	//}
	// 패킷 조립 끝났으면 락 풀기.

	if(goodToProcess) {
		processPacket(id);
	}
	
	return;
}

void Server::processIo(ClientId id, OverExPtr overExPtr, int bytesTransferred)
{
	switch(overExPtr->ioType) {
	case EventType::Read:
		assembleAndProcessPacket(id, bytesTransferred);
		recvPost(id);
		break;

	case EventType::Write:
		break;

	case EventType::NpcMove:
	{
		Event event;
		event.objectId = id;
		event.type = EventType::NpcMove;
		event.startTime = std::chrono::high_resolution_clock::now();
		processEvent(event);
		break;
	}
	case EventType::NpcDetectPlayer:
		break;

	case EventType::Heal:
		break;

	default:
		spdlog::warn("Unhandled IoType from Client Id #{0}", id);
		break;
	}

	if(overExPtr != nullptr && overExPtr->ioType != EventType::Read) {
		mOverExMemoryPool.dealloc(overExPtr);
	}
}

void Server::processPacket(ClientId id)
{
	MessageType msgType;
	//Waiting* waiting = nullptr;
	Client* client = nullptr;
	ClientId newId = 0;

	// waiting client
	//if(isWaitingClient(id)) {
	//	WaitingIdTable::accessor accessor;
	//	findWaiting(accessor, id);
	//	waiting = mWaitingPool.unsafeAt(accessor->second);
	//	msgType = static_cast<MessageType>(waiting->packetBuffer[sizeof(MessageSize)]);
	//	accessor.release();
	//}
	// 이미 접속된 client
	//else {
		ClientIdTable::const_accessor accessor;
		if (!mClientIdTable.find(accessor, id)) {
			spdlog::warn("Can't find client in ClientIdTable. (Server::processPacket())");
			return;
		}
		client = mClientPool.unsafeAt(accessor->second);
		msgType = static_cast<MessageType>(client->packetBuffer[sizeof(MessageSize)]);
		accessor.release();
	//}

	// 오면 안되는 패킷이 오면 처리하지 않는다.
	// MessageType::CSLogin은 Waiting client만 보낸다.
	//if (waiting != nullptr) {
	//	if (msgType != MessageType::CSLogin)
	//		return id;
	//}
	//else if (client != nullptr) {
	//	if (msgType == MessageType::CSLogin)
	//		return id;
	//}
	//else {
	//	return id;
	//}


	switch(msgType) {
	case MessageType::CSLogin:
		processLogin(id);
		break;

	case MessageType::CSLogout:
		processLogout(id);
		break;

	case MessageType::CSMove:
		processMove(id);
		break;

	case MessageType::CSAttack:
		processMove(id);
		break;

	case MessageType::CSChat:
		processChat(id);
		break;

	default:
		spdlog::warn("Unhandled type of Message. (Server::processPacket())");
		break;
	}

	return;
}

void Server::processEvent(Event & event)
{
	switch (event.type) {
	case EventType::Heal:
		break;

	case EventType::NpcDetectPlayer:
		break;

	case EventType::NpcMove:
	{
		//// 주변에 클라이언트가 있으면 random move
		//bool isPlayerNear = false;
		//// view list를 읽어온다.
		//ObjectIdTable::accessor objectAccessor;
		//if (!findObject(objectAccessor, event.objectId)) {
		//	objectAccessor.release();
		//}
		//Object* myObject = mObjectPool.unsafeAt(objectAccessor->second);
		//SectorId sectorId = myObject->sectorId;
		//std::unordered_set<ObjectId> myViewList = myObject->viewList;
		//objectAccessor.release();

		//// view list에 있는 오브젝트 중 플레이어가 있는지 검사한다.
		//for (auto& otherObjectId : myViewList) {
		//	if (!findObject(objectAccessor, otherObjectId)) {
		//		objectAccessor.release();
		//	}
		//	Object* otherObject = mObjectPool.unsafeAt(objectAccessor->second);
		//	// 근처에 플레이어가 있다면 random move를 시켜준다.
		//	if (otherObject->objectType == ObjectType::Player) {
		//		isPlayerNear = true;
		//		objectAccessor.release();
		//		break;
		//	}
		//	objectAccessor.release();
		//}

		//if (isPlayerNear) {
		//	moveObject(INVALID_SOCKET, event.objectId, static_cast<Dir>(rand() % 4), getFirstViewSector(sectorId));
		//	addTimer(event.objectId, EventType::NpcMove, std::chrono::high_resolution_clock::now() + 1s);
		//	spdlog::info("NpcMove added to timer.");
		//}
		ObjectIdTable::accessor objectAccessor;
		if (!findObject(objectAccessor, event.objectId)) {
			objectAccessor.release();
		}
		Object* object = mObjectPool.unsafeAt(objectAccessor->second);
		SectorId sectorId = object->sectorId;
		if (!object->isSleep) {
			objectAccessor.release();
			moveObject(INVALID_SOCKET, event.objectId, static_cast<Dir>(rand() % 4), getFirstViewSector(sectorId));
			addTimer(event.objectId, EventType::NpcMove, std::chrono::high_resolution_clock::now() + 1s);
			spdlog::info("NpcMove added to timer.");
		}
		objectAccessor.release();
		break;
	}
	default:
		spdlog::error("Unhandled event type error.");
	#if DEBUG
		while (true);
	#endif
		break;
	}
}

void Server::addTimer(ObjectId objectId, EventType eventType, std::chrono::steady_clock::time_point startTime)
{
	mTimerQueue.emplace(Event { objectId, eventType, startTime });
}

void Server::processLogin(ClientId id)
{
	ClientIdTable::accessor clientAccessor;
	if (!findClient(clientAccessor, id)) {
		return;
	}
	Client* client = mClientPool.unsafeAt(clientAccessor->second);

	size_t strSize = static_cast<MessageSize>(client->packetBuffer[0]) - MessageHeaderSize;
	wcsncpy_s(
		client->idStr,
		10,
		(WCHAR*)(client->packetBuffer + MessageHeaderSize),
		strSize / sizeof(wchar_t)
	);
	std::wstring idStr { client->idStr };
	clientAccessor.release();
	QueryMessage query;
	query.clientId = id;
	query.type = QueryType::Login;
	query.query = mDatabase.sqlBuildQueryLogin(const_cast<wchar_t*>(idStr.c_str()));
	mSqlQueryQueue.push(query);
	client = nullptr;

	return;
}

void Server::processLogout(ClientId id)
{
	shutdownClient(id);
}

void Server::processMove(ClientId id)
{
	ClientIdTable::accessor clientAccessor;
	ObjectIdTable::accessor objectAccessor;
	Client* client;
	ObjectId objectId;
	CSMove* packet;
	SOCKET socket = INVALID_SOCKET;
	Dir dir;
	SectorId sectorId;

	if (findClient(clientAccessor, id)) {
		client = mClientPool.unsafeAt(clientAccessor->second);
		packet = reinterpret_cast<CSMove*>(client->packetBuffer);
		//TODO: view list안에 있는 사람들이랑 비교해야함.
		dir = static_cast<Dir>(packet->dir);
		// 네개의 섹터에 있는 모든 클라이언트랑 비교를 해야함.
		socket = client->socket;
		objectId = client->objectId;
		clientAccessor.release();
	}
	else {
		clientAccessor.release();
		return;
	}
	if (!findObject(objectAccessor, objectId))
		return;
	sectorId = getFirstViewSector(mObjectPool.unsafeAt(objectAccessor->second)->sectorId);
	objectAccessor.release();
	moveObject(socket, objectId, dir, sectorId);
}

void Server::processChat(ClientId id) {}

//TODO: 첫번째 인자는 ObjectId로 받아야 함. ai들도 다 이 함수 써서 움직일 거니까
// 주위 sector에 있는 Object들과 검사 후 이동시켜주고 Sector를 업데이터해준다.
void Server::moveObject(SOCKET myClientSocket, ObjectId objectId, Dir dir, SectorId firstSector)
{
	// 움직일 수 있나부터 검사.
	SectorId sector1 = firstSector;

	std::unordered_set<ObjectId> sector;
	//std::unordered_set<ObjectId> addMe;
	//std::unordered_map<ObjectId, Object> addOther;
	//std::unordered_set<ObjectId> removeOther;
	//std::unordered_map<ObjectId> removeMe;

	std::unordered_set<ObjectId> oldViewList;
	std::unordered_set<ObjectId> newViewList;
	

	ObjectIdTable::accessor myObjectAccessor;
	Object* myObject = nullptr;
	WORD myX = -1;
	WORD myY = -1;
	WORD myOldX = -1;
	WORD myOldY = -1;
	SectorId mySectorId = -1;
	ObjectType myObjectType;
	ClientId myObjectClientId = 0;
	SOCKET otherClientSocket = INVALID_SOCKET;
	bool isPlayerNear = false;

	if (!findObject(myObjectAccessor, objectId)) {
		myObjectAccessor.release();
		return;
	}
	myObject = mObjectPool.unsafeAt(myObjectAccessor->second);
	myX = myObject->xPos;
	myY = myObject->yPos;
	mySectorId = myObject->sectorId;
	myObjectType = myObject->objectType;
	myObjectClientId = myObject->clientId;
	oldViewList = myObject->viewList;
	myObjectAccessor.release();

	// 이동이 가능한 상태인지 체크한다.
	if (dir == Dir::Down && myY == 0)
		return;
	if (dir == Dir::Up && myY == MAP_HEIGHT - 1)
		return;
	if (dir == Dir::Right && myX == MAP_WIDTH - 1)
		return;
	if (dir == Dir::Left && myX == 0)
		return;

	for (int i = 0; i < 4; ++i) {
		int offset = (i / 2) * MAX_XSECTOR + (i % 2);

		SectorTable::accessor sectorAccessor;
		if (!findSector(sectorAccessor, sector1 + offset)) {
			sectorAccessor.release();
			return;
		}
		// 현재 이 섹터에 있는 object들을 가져온다.
		sector.clear();
		sector = sectorAccessor->second;
		sectorAccessor.release();

		// 섹터에 있는 object들을 대상으로 거리 체크를 해가며 old, new view list에 넣는다.
		for (auto& otherObjectId : sector) {
			if (otherObjectId == objectId) continue;
			ObjectIdTable::accessor objectAccessor;
			if (!findObject(objectAccessor, otherObjectId)) {
				objectAccessor.release();
				continue;
			}
			Object* otherObject = mObjectPool.unsafeAt(objectAccessor->second);
			WORD otherXPos = otherObject->xPos;
			WORD otherYPos = otherObject->yPos;
			ObjectType otherObjectType = otherObject->objectType;
			objectAccessor.release();

			if (std::abs(otherXPos - myX) <= VIEW_RANGE &&
				std::abs(otherYPos - myY) <= VIEW_RANGE) {
				// 장애물이 경로에 있는지 검사한다.
				if (otherObject->objectType == ObjectType::Obstacle) {
					if (dir == Dir::Down) {
						if (myX == otherXPos && myY == otherYPos + 1) {
							// Can't move
							return;
						}
					}
					else if (dir == Dir::Up) {
						if (myX == otherXPos && myY == otherYPos - 1) {
							// Cna't move
							return;
						}
					}
					else if (dir == Dir::Right) {
						if (myX == otherXPos - 1 && myY == otherYPos) {
							// Cna't move
							return;
						}

					}
					else if (dir == Dir::Left) {
						if (myX == otherXPos + 1 && myY == otherYPos) {
							// Can't move
							return;
						}
					}
				}
				// 장애물이 없으면 계속...
				// 범위 내에 있다면 old view list에 추가해준다.
				//oldViewList.insert(otherObjectId);

				// 이동 한 위치에서도 view range안에 있다면 new view list에 추가해준다.
				if (dir == Dir::Down) {
					if (std::abs(otherYPos - (myY - 1)) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
				else if (dir == Dir::Up) {
					if (std::abs(otherYPos - (myY + 1)) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
				else if (dir == Dir::Left) {
					if (std::abs(otherXPos - (myX - 1)) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
				else if (dir == Dir::Right) {
					if (std::abs(otherXPos - (myX + 1)) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
			}
			else {
				if (dir == Dir::Down) {
					if (std::abs(otherXPos - myX) <= VIEW_RANGE &&
						std::abs(otherYPos - (myY - 1)) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
				else if (dir == Dir::Up) {
					if (std::abs(otherXPos - myX) <= VIEW_RANGE &&
						std::abs(otherYPos - (myY + 1)) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
				else if (dir == Dir::Left) {
					if (std::abs(otherXPos - (myX - 1)) <= VIEW_RANGE &&
						std::abs(otherYPos - myY) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
				else if (dir == Dir::Right) {
					if (std::abs(otherXPos - (myX + 1)) <= VIEW_RANGE &&
						std::abs(otherYPos - myY) <= VIEW_RANGE) {
						newViewList.insert(otherObjectId);
					}
				}
			}
			if (newViewList.count(otherObjectId) != 0 && otherObjectType == ObjectType::Player)
				isPlayerNear = true;

			// view list에 추가가 됐으면 각자 view list에 추가해준다.
			// 내가 플레이어고 상대가 npc라면 잠에서 깨워준다.
			if (newViewList.count(otherObjectId) != 0) {
				// 내 오브젝트 view list에 추가
				if (!findObject(objectAccessor, objectId)) {
					objectAccessor.release();
				}
				myObject = mObjectPool.unsafeAt(objectAccessor->second);
				if (myObject->viewList.count(otherObjectId) == 0)
					myObject->viewList.insert(otherObjectId);
				objectAccessor.release();

				// 상대 오브젝트 view list에 추가
				if (!findObject(objectAccessor, otherObjectId)) {
					objectAccessor.release();
				}
				otherObject = mObjectPool.unsafeAt(objectAccessor->second);
				if (otherObject->viewList.count(objectId) == 0)
					otherObject->viewList.insert(objectId);
				objectAccessor.release();

				// 여기서 상대가 npc고 내가 클라이언트면 깨워주고 자고있었으면 timer에 추가해준다.
				if (myObjectType == ObjectType::Player && otherObjectType != ObjectType::Player && otherObjectType != ObjectType::Obstacle) {
					if (!findObject(objectAccessor, otherObjectId)) {
						objectAccessor.release();
						continue;
					}
					otherObject = mObjectPool.unsafeAt(objectAccessor->second);
					if (otherObject->isSleep) {
						otherObject->isSleep = false;
						addTimer(otherObjectId, EventType::NpcMove, std::chrono::high_resolution_clock::now());
						spdlog::info("NpcMove added to timer.");
					}
					objectAccessor.release();
				}
			}
		}
	}
	//// 모든 sector 다 검사했고 이동 가능하니 움직여주자.
	if (!findObject(myObjectAccessor, objectId)) {
		myObjectAccessor.release();
		return;
	}
	myObject = mObjectPool.unsafeAt(myObjectAccessor->second);
	if (dir == Dir::Up) {
		myObject->yPos++;
		myY++;
	}
	else if (dir == Dir::Down) {
		myObject->yPos--;
		myY--;
	}
	else if (dir == Dir::Right) {
		myObject->xPos++;
		myX++;
	}
	else if (dir == Dir::Left) {
		myObject->xPos--;
		myX--;
	}

	myObject->sectorId = getSectorId(myX, myY);
	myObjectAccessor.release();
	// 옮긴 이후에 섹터 아이디가 변경되었을 수 있으니 업데이트 해 줍니다.
	updateSector(objectId, myX, myY, mySectorId);

	// 내가 npc인데 주위에 플레이어가 있을때
	if (myObjectType != ObjectType::Player && myObjectType != ObjectType::Obstacle) {
		if (isPlayerNear) {
			ObjectIdTable::accessor objectAccessor;
			if (!findObject(objectAccessor, objectId)) {
				objectAccessor.release();
				return;
			}
			myObject = mObjectPool.unsafeAt(objectAccessor->second);
			// 자던중이면 깨워주고 timer에 추가한다
			if (myObject->isSleep) {
				myObject->isSleep = false;
				addTimer(objectId, EventType::NpcMove, std::chrono::high_resolution_clock::now());
				spdlog::info("NpcMove added to timer.");
			}
			objectAccessor.release();
		}
		else {
			ObjectIdTable::accessor objectAccessor;
			if (!findObject(objectAccessor, objectId)) {
				objectAccessor.release();
				return;
			}
			myObject = mObjectPool.unsafeAt(objectAccessor->second);
			myObject->isSleep = true;
			spdlog::info("Go to sleep.");
			objectAccessor.release();
		}
	}

	// 이동후의 view list부터 살펴봅니다.
	for (auto& otherObjectId : newViewList) {
		if (objectId == otherObjectId) continue;

		// old와 new에 동시에 존재한다면 position를 보내주면 됩니다.
		if (oldViewList.count(otherObjectId) != 0) {
			ObjectIdTable::accessor objectAccessor;

			// 상대 오브젝트가 클라이언트에 속해있다면
			if (!findObject(objectAccessor, otherObjectId)) {
				objectAccessor.release();
				continue;
			}
			Object* otherObject = mObjectPool.unsafeAt(objectAccessor->second);
			ObjectType otherObjectType = otherObject->objectType;
			WORD otherObjectXPos = otherObject->xPos;
			WORD otherObjectYPos = otherObject->yPos;
			ClientId otherObjectClientId = otherObject->clientId;
			SOCKET otherObjectSocket = otherObject->socket;
			objectAccessor.release();

			if (otherObjectType == ObjectType::Player) {
				// 상대에게 내 오브젝트 정보를 전달한다.
				SCPositionInfo packet(objectId, myX, myY);
				sendPost(otherObjectSocket, otherObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
				//sendPost(otherObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
			}

			// 내 오브젝트가 클라이언트에 속해있다면
			if (myObjectType == ObjectType::Player) {
				// 나에게 상대 오브젝트 정보를 전달한다.
				SCPositionInfo packet(otherObjectId, otherObjectXPos, otherObjectYPos);
				sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
				//sendPost(myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
			}
		}
		// old에 없고 new에만 존재한다면 add를 보내주면 됩니다.
		else {
			ObjectIdTable::accessor objectAccessor;

			// 상대 오브젝트가 클라이언트에 속해있다면
			if (!findObject(objectAccessor, otherObjectId)) {
				objectAccessor.release();
				continue;
			}
			Object* otherObject = mObjectPool.unsafeAt(objectAccessor->second);
			ObjectType otherObjectType = otherObject->objectType;
			WORD otherObjectXPos = otherObject->xPos;
			WORD otherObjectYPos = otherObject->yPos;
			ClientId otherObjectClientId = otherObject->clientId;
			SOCKET otherObjectSocket = otherObject->socket;
			objectAccessor.release();

			if (otherObjectType == ObjectType::Player) {
				// 상대에게 내 오브젝트 정보를 전달한다.
				SCAddObject packet(objectId, myObjectType, myX, myY);
				sendPost(otherObjectSocket, otherObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
				//sendPost(otherObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
			}

			// 내 오브젝트가 클라이언트에 속해있다면
			if (myObjectType == ObjectType::Player) {
				// 나에게 상대 오브젝트 정보를 전달한다.
				SCAddObject packet(otherObjectId, otherObjectType, otherObjectXPos, otherObjectYPos);
				sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
				//sendPost(myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
			}
		}
	}
	for (auto& otherObjectId : oldViewList) {
		if (otherObjectId == objectId) continue;
		// old에 있는데 new에 없다면 remove를 보내주면 됩니다.
		if (newViewList.count(otherObjectId) == 0) {
			ObjectIdTable::accessor objectAccessor;
			if (!findObject(objectAccessor, otherObjectId)) {
				objectAccessor.release();
				continue;
			}
			Object* otherObject = mObjectPool.unsafeAt(objectAccessor->second);
			ObjectType otherObjectType = otherObject->objectType;
			ClientId otherObjectClientId = otherObject->clientId;
			SOCKET otherObjectSocket = otherObject->socket;
			otherObject->viewList.erase(objectId);
			objectAccessor.release();

			if (!findObject(objectAccessor, objectId)) {
				objectAccessor.release();
				continue;
			}
			myObject = mObjectPool.unsafeAt(objectAccessor->second);
			myObject->viewList.erase(otherObjectId);
			objectAccessor.release();

			if (otherObjectType == ObjectType::Player) {
				SCRemoveObject packet(objectId);
				sendPost(otherObjectSocket, otherObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
				//sendPost(otherObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
			}

			if (myObjectType == ObjectType::Player) {
				SCRemoveObject packet(otherObjectId);
				sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
				//sendPost(myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
			}
		}
	}

	
	if (myObjectType == ObjectType::Player) {
		SCPositionInfo packet(objectId, myX, myY);
		sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
	}
	//sendPost(myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);

	//// 움직이려는 object에 대한 권한을 얻음.
	//if (!findObject(myObjectAccessor, objectId)) {
	//	return;
	//}
	//	//spdlog::info("object{0} locked.", objectId);
	//myObject = mObjectPool.unsafeAt(myObjectAccessor->second);
	//myX = myObject->xPos, myOldX = myObject->xPos;
	//myY = myObject->yPos, myOldY = myObject->yPos;
	//myObjectType = myObject->objectType;
	//myObjectClientId = myObject->clientId;
	//myObjectAccessor.release();

	//if (dir == Dir::Down && myY == 0)
	//	return;
	//if (dir == Dir::Up && myY == MAP_HEIGHT - 1)
	//	return;
	//if (dir == Dir::Right && myX == MAP_WIDTH - 1)
	//	return;
	//if (dir == Dir::Left && myX == 0)
	//	return;

	//for (int i = 0; i < 4; ++i) {
	//	int offset = (i / 2) * MAX_XSECTOR + (i % 2);

	//	SectorTable::accessor sectorAccessor;
	//	if (!findSector(sectorAccessor, sector1 + offset))
	//		return;
	//	sector.clear();
	//	sector = sectorAccessor->second;
	//	sectorAccessor.release();

	//	//spdlog::info("Sector{0} locked.{1}", sector1 + offset, objectId);
	//	for (auto& otherObjectId : sector) {
	//		// 자기 자신은 넘어감.
	//		if (otherObjectId == objectId)
	//			continue;
	//		// Sector안에 있는 모든 오브젝트의 위치를 비교해서 움직이려는 위치와 하나라도 같으면 못움직임.
	//		ObjectIdTable::accessor objectAccessor;
	//		if (!findObject(objectAccessor, otherObjectId)) {
	//			objectAccessor.release();
	//			continue;
	//		}
	//		//spdlog::info("Object{0} locked.", otherObjectId);
	//		Object* object = mObjectPool.unsafeAt(objectAccessor->second);
	//		if (!object->initialized) {
	//			objectAccessor.release();
	//			continue;
	//		}
	//		otherXPos = object->xPos;
	//		otherYPos = object->yPos;

	//		// 화면에 표시될 major view list와 다음 이동으로 major view list에 포함될 수 있는
	//		// minor view list들을 채운다.
	//		if (std::abs(otherXPos - myX) <= VIEW_RANGE &&
	//			std::abs(otherYPos - myY) <= VIEW_RANGE) {
	//			oldViewList.insert(otherObjectId);
	//		}

	//		objectAccessor.release();


	//		// 장애물이 경로에 있는지 검사한다.
	//		if (object->objectType == ObjectType::Obstacle) {

	//			if (dir == Dir::Down) {
	//				if (myX == otherXPos && myY == otherYPos + 1) {
	//					// Can't move
	//					return;
	//				}
	//			}
	//			else if (dir == Dir::Up) {
	//				if (myX == otherXPos && myY == otherYPos - 1) {
	//					// Cna't move
	//					return;
	//				}
	//			}
	//			else if (dir == Dir::Right) {
	//				if (myX == otherXPos - 1 && myY == otherYPos) {
	//					// Cna't move
	//					return;
	//				}

	//			}
	//			else if (dir == Dir::Left) {
	//				if (myX == otherXPos + 1 && myY == otherYPos) {
	//					// Can't move
	//					return;
	//				}
	//			}
	//		}
	//	}
	//	//sectorAccessor.release();
	//	//spdlog::info("Sector unlocked.objectId{0}", objectId);
	//}

	//findObject(myObjectAccessor, objectId);
	//myObject = mObjectPool.unsafeAt(myObjectAccessor->second);
	//if (!myObject->initialized) {
	//	myObjectAccessor.release();
	//	return;
	//}
	//mySectorId = myObject->sectorId;
	//// 모든 sector 다 검사했고 이동 가능하니 움직여주자.
	////spdlog::info("moved up.");
	//if (dir == Dir::Up) {
	//	//spdlog::info("Moved up.");
	//	myObject->yPos++;
	//	myY++;
	//}
	//else if (dir == Dir::Down) {
	//	myObject->yPos--;
	//	myY--;
	//}
	//else if (dir == Dir::Right) {
	//	myObject->xPos++;
	//	myX++;
	//}
	//else if (dir == Dir::Left) {
	//	myObject->xPos--;
	//	myX--;
	//}
	//
	//for (int i = 0; i < 4; ++i) {
	//	int offset = (i / 2) * MAX_XSECTOR + (i % 2);

	//	SectorTable::accessor sectorAccessor;
	//	if (!findSector(sectorAccessor, sector1 + offset)) {
	//		sectorAccessor.release();
	//		return;
	//	}
	//	sector.clear();
	//	sector = sectorAccessor->second;
	//	sectorAccessor.release();

	//	//spdlog::info("Sector{0} locked.{1}", sector1 + offset, objectId);
	//	for (auto& otherObjectId : sector) {
	//		// 자기 자신은 넘어감.
	//		if (otherObjectId == objectId)
	//			continue;
	//		// Sector안에 있는 모든 오브젝트의 위치를 비교해서 움직이려는 위치와 하나라도 같으면 못움직임.
	//		ObjectIdTable::accessor objectAccessor;
	//		if (!findObject(objectAccessor, otherObjectId)) {
	//			objectAccessor.release();
	//			continue;
	//		}
	//		//spdlog::info("Object{0} locked.", otherObjectId);
	//		Object* object = mObjectPool.unsafeAt(objectAccessor->second);
	//		if (!object->initialized) {
	//			objectAccessor.release();
	//			continue;
	//		}
	//		otherXPos = object->xPos;
	//		otherYPos = object->yPos;

	//		// 화면에 표시될 major view list와 다음 이동으로 major view list에 포함될 수 있는
	//		// minor view list들을 채운다.
	//		if (std::abs(otherXPos - myX) <= VIEW_RANGE &&
	//			std::abs(otherYPos - myY) <= VIEW_RANGE) {
	//			newViewList.insert(otherObjectId);
	//		}
	//		objectAccessor.release();
	//	}
	//}

	//myObjectAccessor.release();
	//updateSector(objectId, myX, myY, mySectorId);

	//for (auto& otherObjectId : newViewList) {
	//	if (oldViewList.count(otherObjectId) == 0) {
	//		// add
	//		if (!findObject(myObjectAccessor, otherObjectId)) {
	//			myObjectAccessor.release();
	//			continue;
	//		}
	//		Object* otherObject = mObjectPool.unsafeAt(myObjectAccessor->second);
	//		if (!otherObject->initialized) {
	//			myObjectAccessor.release();
	//			continue;
	//		}
	//		ClientId otherClientId = otherObject->clientId;
	//		WORD otherXPos = otherObject->xPos;
	//		WORD otherYPos = otherObject->yPos;
	//		ObjectType otherObjectType = otherObject->objectType;
	//		myObjectAccessor.release();
	//		
	//		if (myObjectType == ObjectType::Player) {
	//			SCAddObject packet(otherObjectId, otherObjectType, otherXPos, otherYPos);
	//			sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//		if (otherObjectType == ObjectType::Player) {
	//			SCAddObject packet(objectId, myObjectType, myX, myY);
	//			sendPost(otherClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//	}
	//	else {
	//		// position
	//		ObjectIdTable::accessor objectAccessor;
	//		if (!findObject(objectAccessor, otherObjectId)) {
	//			objectAccessor.release();
	//			continue;
	//		}
	//		Object* otherObject = mObjectPool.unsafeAt(objectAccessor->second);
	//		if (!otherObject->initialized) {
	//			objectAccessor.release();
	//			continue;
	//		}
	//		ClientId otherClientId = otherObject->clientId;
	//		WORD otherXPos = otherObject->xPos;
	//		WORD otherYPos = otherObject->yPos;
	//		ObjectType otherObjectType = otherObject->objectType;
	//		objectAccessor.release();

	//		if (myObjectType == ObjectType::Player) {
	//			SCPositionInfo packet(otherObjectId, otherXPos, otherYPos);
	//			sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//		if (otherObjectType == ObjectType::Player) {
	//			SCPositionInfo packet(objectId, myX, myY);
	//			sendPost(otherClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//	}
	//}
///////////////////////////////////////////////////////////////////////

	//for (auto& otherObjectId : newViewList) {
	//	if (oldViewList.count(otherObjectId) != 0) {
	//		// position
	//		if (!findObject(myObjectAccessor, otherObjectId)) {
	//			myObjectAccessor.release();
	//			continue;
	//		}
	//		Object* otherObject = mObjectPool.unsafeAt(myObjectAccessor->second);
	//		ClientId otherClientId = otherObject->clientId;
	//		WORD otherXPos = otherObject->xPos;
	//		WORD otherYPos = otherObject->yPos;
	//		ObjectType otherObjectType = otherObject->objectType;
	//		myObjectAccessor.release();

	//		if (myObjectType == ObjectType::Player) {
	//			SCPositionInfo packet(otherObjectId, otherXPos, otherYPos);
	//			sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//		if (otherObjectType == ObjectType::Player) {
	//			SCPositionInfo packet(objectId, myX, myY);
	//			sendPost(otherClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//	}
	//}

	//for (auto& otherObjectId : oldViewList) {
	//	if (newViewList.count(otherObjectId) == 0) {
	//		// remove
	//		if (!findObject(myObjectAccessor, otherObjectId)) {
	//			myObjectAccessor.release();
	//			continue;
	//		}
	//		Object* otherObject = mObjectPool.unsafeAt(myObjectAccessor->second);
	//		if (!otherObject->initialized) {
	//			myObjectAccessor.release();
	//			continue;
	//		}
	//		ClientId otherClientId = otherObject->clientId;
	//		ObjectType otherObjectType = otherObject->objectType;
	//		myObjectAccessor.release();

	//		if (myObjectType == ObjectType::Player) {
	//			SCRemoveObject packet(otherObjectId);
	//			sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//		if (otherObjectType == ObjectType::Player) {
	//			SCRemoveObject packet(objectId);
	//			sendPost(otherClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//		}
	//	}
	//}

	//for (auto& other : minorViewList) {
	//	// 일단 락을 걸어서 체크를 해줘볼까
	//	if (std::abs(other.second.xPos - myX) <= VIEW_RANGE &&
	//		std::abs(other.second.yPos - myY) <= VIEW_RANGE) {
	//		// SCAddObject
	//		ObjectType otherObjectType = other.second.objectType;
	//		WORD otherXPos = other.second.xPos;
	//		WORD otherYPos = other.second.yPos;
	//		ClientId otherObjectClientId = other.second.clientId;
	//		SCAddObject packet1(other.second.objectId, otherObjectType, otherXPos, otherYPos);
	//		sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet1), packet1.size);

	//		// if otherObject == Client
	//		// SCAddObject
	//		if (otherObjectType == ObjectType::Player) {
	//			SCAddObject packet2(objectId, myObjectType, myX, myY);
	//			ClientIdTable::accessor clientAccessor;
	//			if (!findClient(clientAccessor, otherObjectClientId)) {
	//				clientAccessor.release();
	//				continue;
	//			}
	//			Client* otherClient = mClientPool.unsafeAt(clientAccessor->second);
	//			SOCKET otherClientSocket = otherClient->socket;
	//			clientAccessor.release();
	//			sendPost(otherClientSocket, otherObjectClientId, reinterpret_cast<const char*>(&packet2), packet2.size);
	//		}
	//	}
	//}
	//for (auto& other : majorViewList) {
	//	ClientId otherObjectClientId = other.second.clientId;
	//	if (std::abs(other.second.xPos - myX) <= VIEW_RANGE &&
	//		std::abs(other.second.yPos - myY) <= VIEW_RANGE) {
	//		// SCPosition
	//		SCPositionInfo packet1(other.second.objectId, other.second.xPos, other.second.yPos);
	//		sendPost(myClientSocket, myObjectClientId,  reinterpret_cast<const char*>(&packet1), packet1.size);

	//		// if otherObject == Client
	//		// SCPosition
	//		if (other.second.objectType == ObjectType::Player) {
	//			SCPositionInfo packet2(objectId, myX, myY);
	//			ClientIdTable::accessor clientAccessor;
	//			if (!findClient(clientAccessor, otherObjectClientId)) {
	//				clientAccessor.release();
	//				continue;
	//			}
	//			Client* otherClient = mClientPool.unsafeAt(clientAccessor->second);
	//			SOCKET otherClientSocket = otherClient->socket;
	//			clientAccessor.release();
	//			sendPost(otherClientSocket, otherObjectClientId, reinterpret_cast<const char*>(&packet2), packet2.size);
	//		}
	//	}
	//	else {
	//		// SCRemove
	//		SCRemoveObject packet1(other.second.objectId);
	//		myObjectAccessor.release();
	//		sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet1), packet1.size);

	//		// if otherObject == Client
	//		// SCRemove
	//		if (other.second.objectType == ObjectType::Player) {
	//			SCRemoveObject packet2(objectId);
	//			ClientIdTable::accessor clientAccessor;
	//			if (!findClient(clientAccessor, otherObjectClientId)) {
	//				clientAccessor.release();
	//				continue;
	//			}
	//			Client* otherClient = mClientPool.unsafeAt(clientAccessor->second);
	//			SOCKET otherClientSocket = otherClient->socket;
	//			ClientId otherClientId = clientAccessor->first;
	//			clientAccessor.release();
	//			sendPost(otherClientSocket, otherClientId, reinterpret_cast<const char*>(&packet2), packet2.size);
	//		}
	//	}
	//}
	// SCPosition
	// 자신이 움직인 정보를 보내준다.




	//// sector 업데이트 해준다.
	//// 이 함수는 accessor가 권한을 얻고있는 중에 써야한다.

	//// 이동 했고 sector도 update해줬다.
	//// 이제 새로운 sector내에 있는 client들을 비교해 어떤 패킷을 보내줄지 정한다.
	//for (int i = 0; i < 4; ++i) {
	//	int offset = (i / 2) * MAX_XSECTOR + (i % 2);
	//	SectorTable::accessor sectorAccessor;
	//	if (!findSector(sectorAccessor, sector1 + offset))
	//		return;
	//	sector.clear();
	//	sector = sectorAccessor->second;
	//	sectorAccessor.release();
	//	//spdlog::info("Sector{0} locked. {1}", sector1 + offset, objectId);

	//	for (auto& otherObjectId : sector) {
	//		// 자기 자신이면 패스
	//		if (objectId == otherObjectId)
	//			continue;
	//		ObjectIdTable::accessor objectAccessor;
	//		if (!findObject(objectAccessor, otherObjectId)) {
	//			objectAccessor.release();
	//			continue;
	//		}
	//		//spdlog::info("Object{0} locked.", otherObjectId);
	//		Object* otherObject = mObjectPool.unsafeAt(objectAccessor->second);
	//		ClientId otherClientId = otherObject->clientId;
	//		short otherXPos = otherObject->xPos;
	//		short otherYPos = otherObject->yPos;
	//		ObjectType otherType = otherObject->objectType;

	//		objectAccessor.release();
	//		// 둘다 클라이언트가 아니면 패스
	//		if (otherClientId == 0 /*&& myObject->clientId == 0*/)
	//			continue;


	//		WORD otherClientXPos = otherXPos;
	//		WORD otherClientYPos = otherYPos;
	//		// 시야 안에 있는 client가 있으면 해당 클라의 view list에 자신이 있는지 확인한다.
	//		// 있으면 SCPositionInfo, 없으면 SCAddObject
	//		/*ObjectIdTable::const_accessor constObjectAccessor;
	//		mObjectIdTable.find(constObjectAccessor, objectId);
	//		myObject = mObjectPool.unsafeAt(constObjectAccessor->second);*/
	//		
	//		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 여기서 부터가 데드락을 유발한다!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//		if (std::abs(otherClientXPos - myX) <= VIEW_RANGE &&
	//			std::abs(otherClientYPos - myY) <= VIEW_RANGE) {

	//			// 1. 상대가 클라이언트일때
	//			if (otherType == ObjectType::Player) {
	//				// 1. 상대 view list에 내가 있으면 SCPositionInfo
	//				ClientIdTable::accessor clientAccessor;
	//				if (!findClient(clientAccessor, otherClientId)) {
	//					clientAccessor.release();
	//					continue;
	//				}
	//				//spdlog::info("Client{0} locked.", otherObject->clientId);
	//				Client* otherClient = mClientPool.unsafeAt(clientAccessor->second);
	//				int viewListCount = otherClient->viewList.count(objectId);
	//				otherClientSocket = otherClient->socket;
	//				clientAccessor.release();
	//				if (viewListCount) {
	//					//spdlog::info("Client unlocked.");
	//					SCPositionInfo packet1(objectId, myX, myY);
	//					sendPost(otherClientSocket, otherClientId, reinterpret_cast<const char*>(&packet1), packet1.size);

	//					// 근데 내가 클라이언트라면 나한테도 보내줘야함. 
	//					if (myObjectType == ObjectType::Player) {
	//						SCPositionInfo packet2(otherObjectId, otherXPos, otherYPos);
	//						sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet2), packet2.size);
	//					}
	//				}

	//				// 2. 상대 view list에 내가 없으면 SCAddObject
	//				if (!findClient(clientAccessor, otherClientId)) {
	//					clientAccessor.release();
	//					continue;
	//				}
	//				//spdlog::info("Client{0} locked.", otherObject->clientId);
	//				otherClient = mClientPool.unsafeAt(clientAccessor->second);
	//				if (!otherClient->viewList.count(objectId)) {
	//					otherClient->viewList.insert(objectId);
	//					otherClientSocket = otherClient->socket;
	//					clientAccessor.release();
	//					//spdlog::info("Client unlocked.");
	//					SCAddObject packet1(objectId, ObjectType::Player, myX, myY);
	//					sendPost(otherClientSocket, otherObject->clientId, reinterpret_cast<const char*>(&packet1), packet1.size);

	//					// 근데 내가 client라면 나한테도 보내줘야함.
	//					if (myObjectType == ObjectType::Player) {
	//						findClient(clientAccessor, myObjectClientId);
	//						//spdlog::info("Client{0} locked.", myObjectClientId);
	//						mClientPool.unsafeAt(clientAccessor->second)->viewList.insert(otherObjectId);
	//						clientAccessor.release();
	//						//spdlog::info("Client unlocked.");
	//						SCAddObject packet2(otherObjectId, otherObject->objectType, otherObject->xPos, otherObject->yPos);
	//						sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet2), packet2.size);
	//					}
	//				}
	//				clientAccessor.release();
	//			}


	//		}
	//		// 시야 안에 없는데 자신이 view list에 있다??
	//		// 그럼 SCRemoveObject
	//		else {

	//			// 상대가 클라이언트라면
	//			if (otherClientId != 0) {
	//				ClientIdTable::accessor clientAccessor;
	//				if (!findClient(clientAccessor, otherClientId)) {
	//					clientAccessor.release();
	//					continue;
	//				}
	//				Client* client = mClientPool.unsafeAt(clientAccessor->second);
	//				// 여기에 있으면 view list에서 삭제하고 클라한테도 삭제하라고 해야함
	//				if (client->viewList.count(objectId)) {
	//					client->viewList.erase(objectId);
	//					otherClientSocket = client->socket;
	//					clientAccessor.release();
	//					SCRemoveObject packet(objectId);
	//					sendPost(otherClientSocket, otherClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//				}
	//				clientAccessor.release();
	//			}
	//			// 내가 클라이언트라면
	//			if (myObjectClientId != 0) {
	//				ClientIdTable::accessor clientAccessor;
	//				if (!findClient(clientAccessor, myObjectClientId)) {
	//					clientAccessor.release();
	//					continue;
	//				}
	//				Client* client = mClientPool.unsafeAt(clientAccessor->second);
	//				if (client->viewList.count(otherObjectId)) {
	//					client->viewList.erase(otherObjectId);
	//					clientAccessor.release();
	//					SCRemoveObject packet(otherObjectId);
	//					sendPost(myClientSocket, myObjectClientId, reinterpret_cast<const char*>(&packet), packet.size);
	//				}
	//				clientAccessor.release();
	//			}
	//		}
	//	}
	//	//sectorAccessor.release();
	//	//spdlog::info("2Sector unlocked.objectId{0}", objectId);
	//}
	//// 내가 움직였으면 이거에 대한 정보를 나한테 보내준다
	//if (myOldX != myObject->xPos || myOldY != myObject->yPos) {
	//	SCPositionInfo packet(myObject->objectId, myObject->xPos, myObject->yPos);
	//	sendPost(myClientSocket, myObject->clientId, reinterpret_cast<const char*>(&packet), packet.size);
	//}

	//spdlog::info("moveObject end. {0}", objectId);
}

void Server::recvPost(ClientId id)
{
	ClientIdTable::accessor clientAccessor;
	//WaitingIdTable::accessor waitingAccessor;
	Client* client = nullptr;
	//Waiting* waiting = nullptr;
	int returnVal = 0;
	//if (!isWaitingClient(id)) {
	if (!findClient(clientAccessor, id))
		return;
	client = mClientPool.unsafeAt(clientAccessor->second);
	returnVal = mIocpHandler.recvPost(client->socket, &client->overEx);
	//}
	//else {
	//	if (findWaiting(waitingAccessor, id)) {
	//		waiting = mWaitingPool.unsafeAt(waitingAccessor->second);
	//		returnVal = mIocpHandler.recvPost(waiting->socket, &waiting->overEx);
	//	}
	//}
	
	//waitingAccessor.release();
	clientAccessor.release();

	if (returnVal != NO_ERROR) {
		int errorCode = WSAGetLastError();

		if (errorCode != ERROR_IO_PENDING) {
			errorDisplay("Failed on Server::recvPost()", errorCode);
			shutdownClient(id);
		}
	}

}

void Server::sendPost(SOCKET socket, ClientId id, const char * buffer, int size)
{
	OverEx* overEx = mOverExMemoryPool.alloc();

	BYTE type = static_cast<BYTE>(buffer[1]);
	if (type > 7 || type < 1)
		spdlog::critical("NANI??? {0}", type);

	int returnVal = 0;
	returnVal = mIocpHandler.sendPost(socket, overEx, buffer, size);

	//waitingAccessor.release();

	if (returnVal != NO_ERROR) {
		int errorCode = WSAGetLastError();

		if (errorCode != ERROR_IO_PENDING) {
			errorDisplay("Failed on Server::sendPost()", errorCode);
			shutdownClient(id);
		}
	}
}

void Server::sendPost(ClientId id, const char* buffer, int size)
{
	OverEx* overEx = mOverExMemoryPool.alloc();

	BYTE type = static_cast<BYTE>(buffer[1]);
	if (type > 7 || type < 1)
		spdlog::critical("NANI??? {0}", type);


	ClientIdTable::accessor clientAccessor;
	//WaitingIdTable::accessor waitingAccessor;
	Client* client = nullptr;
	//Waiting* waiting = nullptr;
	int returnVal = 0;
	//if (!isWaitingClient(id)) {
	if (!findClient(clientAccessor, id))
		return;
	client = mClientPool.unsafeAt(clientAccessor->second);
	returnVal = mIocpHandler.sendPost(client->socket, overEx, buffer, size);
	//}
	//else {
	//	findWaiting(waitingAccessor, id);
	//	waiting = mWaitingPool.unsafeAt(waitingAccessor->second);
	//	returnVal = mIocpHandler.sendPost(waiting->socket, &waiting->overEx, buffer, size);
	//}
	
	clientAccessor.release();
	//waitingAccessor.release();

	if (returnVal != NO_ERROR) {
		int errorCode = WSAGetLastError();

		if (errorCode != ERROR_IO_PENDING) {
			errorDisplay("Failed on Server::sendPost()", errorCode);
			shutdownClient(id);
		}
	}
}

ClientId Server::getAvailableClientId()
{
	ClientId clientId = 1;

	// Find available ClientId.
	while (clientId < MAX_CLIENTS) {
		if (!mClientIdTable.count(clientId)) {
			break;
		}
		++clientId;
	}

	// Check if it's full.
	if (clientId == MAX_CLIENTS) {
		clientId = 0;
	}

	return clientId;
}

//ClientId Server::getAvailableWaitingId()
//{
//	static_assert(MAX_CLIENTS - MAX_WAITING > 0);
//	ClientId clientId = MAX_CLIENTS - MAX_WAITING;
//
//	// Find available ClientId.
//	while (clientId < MAX_CLIENTS) {
//		if (!mWaitingIdTable.count(clientId)) {
//			break;
//		}
//		++clientId;
//	}
//
//	// Check if it's full.
//	if (clientId == MAX_CLIENTS) {
//		clientId = 0;
//	}
//
//	return clientId;
//}

ObjectId Server::getAvailableObjectId()
{
	ObjectId objectId = 1;

	// Find available ObjectId.
	while (objectId < MAX_OBJECT) {
		if (!mObjectIdTable.count(objectId)) {
			break;
		}
		++objectId;
	}

	// Check if it's full.
	if (objectId == MAX_OBJECT) {
		objectId = 0;
	}

	return objectId;
}

// 시야처리를 위한 4개의 섹터 중 가장 좌상단에 위치한 섹터 id를 반환
SectorId Server::getFirstViewSector(WORD xPos, WORD yPos)
{
	int sectorX = xPos / SECTOR_WIDTH;
	int sectorY = yPos / SECTOR_HEIGHT;

	int localX = xPos - SECTOR_WIDTH * sectorX;
	int localY = yPos - SECTOR_HEIGHT * sectorY;

	if (localX < SECTOR_WIDTH / 2) {
		sectorX -= 1;
	}

	if (localY < SECTOR_HEIGHT / 2) {
		sectorY -= 1;
	}

	// Sector가 맵 바깥으로 나가는 경우 조정해줌
	if (sectorX == -1)
		++sectorX;
	if (sectorX == MAX_XSECTOR - 1)
		--sectorX;
	if (sectorY == -1)
		++sectorY;
	if (sectorY == MAX_YSECTOR - 1)
		--sectorY;

	// 네개의 Sector중 왼쪽 아래 sector의 index를 리턴한다
	return sectorX + sectorY * MAX_XSECTOR;
}

SectorId Server::getFirstViewSector(SectorId sectorId)
{
	int sectorX = sectorId % MAX_XSECTOR;
	int sectorY = sectorId / MAX_YSECTOR;

	if (sectorX == -1)
		++sectorX;
	if (sectorX == MAX_XSECTOR - 1)
		--sectorX;
	if (sectorY == -1)
		++sectorY;
	if (sectorY == MAX_YSECTOR - 1)
		--sectorY;

	return sectorX + sectorY * MAX_XSECTOR;
}

// 해당 좌표가 속한 sector의 id를 반환한다
SectorId Server::getSectorId(WORD xPos, WORD yPos)
{
	int sectorX = xPos / SECTOR_WIDTH;
	int sectorY = yPos / SECTOR_HEIGHT;
	return sectorX + sectorY * MAX_XSECTOR;
}

void Server::addObjectToSector(ObjectId id)
{
	ObjectIdTable::accessor objectAccessor;
	findObject(objectAccessor, id);
	Object* object = mObjectPool.unsafeAt(objectAccessor->second);
	SectorId sectorId = getSectorId(object->xPos, object->yPos);
	objectAccessor.release();
	SectorTable::accessor sectorAccessor;
	findSector(sectorAccessor, sectorId);
	sectorAccessor->second.insert(id);
	sectorAccessor.release();
}

void Server::updateSector(ObjectId objectId, short xPos, short yPos, SectorId sectorId)
{
	// object가 가지고있는 sector id가 현재 위치와 다르다면 이전 sector에서 삭제하고 새로운 sector에 등록해준다.
	SectorId newSectorId = getSectorId(xPos, yPos);
	SectorId oldSectorId = sectorId;
	if (newSectorId != oldSectorId) {
		// old sector에서 지워주고
		SectorTable::accessor sectorAccessor;
		findSector(sectorAccessor, oldSectorId);
		if (sectorAccessor->second.count(objectId) != 0) {
			sectorAccessor->second.erase(objectId);
		}
		sectorAccessor.release();
		// new sector에 추가해준다

		if (findSector(sectorAccessor, newSectorId))
			sectorAccessor->second.insert(objectId);
		sectorAccessor.release();
	}
}

void Server::initMonsters()
{
	std::random_device rd;
	std::default_random_engine dre(rd());
	std::uniform_int_distribution<> uid(0, 299);

	// 몬스터 300마리 생성하기
	// 100마리 Hostile
	// 200마리 friendly
	ObjectIdTable::accessor objectAccessor;
	for (int i = 0; i < 500; ++i) {
		Object* friendlyMonster;
		size_t index = mObjectPool.alloc(friendlyMonster);
		friendlyMonster->objectType = ObjectType::FriendlyMonster;
		friendlyMonster->hp = 100;
		//TODO: str, dex 스텟은 나중에
		friendlyMonster->xPos = uid(dre);
		friendlyMonster->yPos = uid(dre);
		friendlyMonster->sectorId = getSectorId(friendlyMonster->xPos, friendlyMonster->yPos);
		
		ObjectId objectId = getAvailableObjectId();
		friendlyMonster->objectId = objectId;
		mObjectIdTable.insert(objectAccessor, objectId);
		objectAccessor->second = index;
		objectAccessor.release();
		
		// sector에 추가해주기
		SectorId sectorId = friendlyMonster->sectorId;
		SectorTable::accessor sectorAccessor;
		if (!findSector(sectorAccessor, sectorId)) {
			sectorAccessor.release();
			continue;
		}
		sectorAccessor->second.insert(objectId);
		sectorAccessor.release();
	}

	for (int i = 0; i < 250; ++i) {
		Object* hostileMonster;
		size_t index = mObjectPool.alloc(hostileMonster);
		hostileMonster->objectType = ObjectType::HostileMonster;
		hostileMonster->hp = 150;
		//TODO: str, dex 스텟은 나중에
		hostileMonster->xPos = uid(dre);
		hostileMonster->yPos = uid(dre);
		hostileMonster->sectorId = getSectorId(hostileMonster->xPos, hostileMonster->yPos);

		ObjectId objectId = getAvailableObjectId();
		hostileMonster->objectId = objectId;
		mObjectIdTable.insert(objectAccessor, objectId);
		objectAccessor->second = index;
		objectAccessor.release();

		// sector에 추가해주기
		SectorId sectorId = hostileMonster->sectorId;
		SectorTable::accessor sectorAccessor;
		if (!findSector(sectorAccessor, sectorId)) {
			sectorAccessor.release();
			continue;
		}
		sectorAccessor->second.insert(objectId);
		sectorAccessor.release();
	}

	spdlog::info("Monster initialized.");
}

Server::Server() :
	mRunning(false)
{
	spdlog::set_level(spdlog::level::info);
	int error = mIocpHandler.init(SERVER_ADDR, PORT, std::bind(&Server::acceptorThreadFunc, this), std::bind(&Server::workerThreadFunc, this));
	if(error != NO_ERROR) {
		errorDisplay("Failed to initialize Iocp", error);
		return;
	}
	mDatabase.connect();
	for (int i = 0; i < MAX_XSECTOR * MAX_YSECTOR; ++i) {
		SectorTable::accessor sectorAccessor;
		mSectorTable.insert(sectorAccessor, i);
	}

	mRunning = true;

	databaseThread = std::thread { [this]() {databaseThreadFunc(); } };
	acceptorThread = std::thread { [this]() {acceptorThreadFunc(); } };
	timerThread = std::thread { [this]() {timerThreadFunc(); } };

	initMonsters();
}

Server::~Server()
{
	mRunning = false;
	mIocpHandler.~IocpHandler();
	WSACleanup();

	acceptorThread.join();
	timerThread.join();
	databaseThread.join();
}
