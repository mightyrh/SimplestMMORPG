#include "stdafx.h"
#include "ScnMgr.h"

// prevTime 을 0으로 초기화 할 수도 있는데 이러면 안됨 0이면 갑자기 날아갈 수도 있음
//TODO: 이거 chrono로 바꾸기.
DWORD prevTime = timeGetTime();
DWORD elapsedTime;
int g_Seq = 0;

int cameraOffset_x = 4;
int cameraOffset_y = 4;
float cameraShakeTick = 0.5f;
float cameraShakeETime = 0.f;
bool cameraShake = false;

int score = 0;

ScnMgr::ScnMgr(int width, int height, const std::string& ip) : 
	mNetworkManager(ip),
	mMyId(0)
{
	mRenderer = NULL;
	mRenderer = new Renderer(width, height);

	// Load textures
	mPlayerTexture = mRenderer->CreatePngTexture("Textures/Player.png");
	mPlayerAttackTexture = mRenderer->CreatePngTexture("Textures/Player_Attack.png");
	mAttackDownTexture = mRenderer->CreatePngTexture("Textures/Attack_Down.png");
	mAttackUpTexture = mRenderer->CreatePngTexture("Textures/Attack_Up.png");
	mAttackLeftTexture = mRenderer->CreatePngTexture("Textures/Attack_Left.png");
	mAttackRightTexture = mRenderer->CreatePngTexture("Textures/Attack_Right.png");
	mPotionTexture = mRenderer->CreatePngTexture("Textures/Potion.png");
	mHealthBarTexture = mRenderer->CreatePngTexture("Textures/Healthbar.png");
	mFriendlyMonsterTexture = mRenderer->CreatePngTexture("Textures/Skeleton.png");
	mHostileMonsterTexture = mRenderer->CreatePngTexture("Textures/Knight.png");
}

void ScnMgr::move(Dir move)
{
	CSMove packet(static_cast<BYTE>(move));

	mNetworkManager.send(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void ScnMgr::processLoginOK(char* message)
{
	auto packet = reinterpret_cast<SCLoginOK*>(message);

	std::scoped_lock<std::mutex> lock(mLock);
	mObjects[packet->id] = new Object;
	mObjects[packet->id]->objectId = packet->id;
	mObjects[packet->id]->xPos = packet->xPos;
	mObjects[packet->id]->yPos = packet->yPos;
	mObjects[packet->id]->level = packet->level;
	mObjects[packet->id]->hp = packet->hp;
	mObjects[packet->id]->exp = packet->exp;
	mObjects[packet->id]->type = ObjectType::Player;
	mMyId = packet->id;

	std::cout << mMyId << std::endl;
}

void ScnMgr::processAddObject(char* message)
{
	auto packet = reinterpret_cast<SCAddObject*>(message);

	std::scoped_lock<std::mutex> lock(mLock);
	Object* object = new Object;
	object->objectId = packet->id;
	object->type = static_cast<ObjectType>(packet->objectType);
	object->xPos = packet->xPos;
	object->yPos = packet->yPos;
	mObjects.insert(std::make_pair(packet->id, object));
	std::cout << "AddObject ID: " << packet->id << " " << packet->xPos << " " << packet->yPos << std::endl;
}

void ScnMgr::processPositionInfo(char* message)
{
	auto packet = reinterpret_cast<SCPositionInfo*>(message);

	int32_t id = packet->id;
	std::scoped_lock<std::mutex> lock(mLock);
	if (mObjects.count(id) != 0) {
		mObjects[id]->xPos = packet->xPos;
		mObjects[id]->yPos = packet->yPos;
		std::cout << "PositionInfo " << packet->xPos << " " << packet->yPos << std::endl;
	}
	else {
		std::cout << "Object: " << id << "was nullptr " << packet->xPos << ", " << packet->yPos << std::endl;
	}

}

void ScnMgr::processRemoveObject(char* message)
{
	auto packet = reinterpret_cast<SCRemoveObject*>(message);

	std::scoped_lock<std::mutex> lock(mLock);
	if (mObjects.count(packet->id) == 0) return;
	if (mObjects[packet->id] == nullptr) return;
	delete mObjects[packet->id];
	mObjects.erase(packet->id);
	std::cout << "RemoveObject " << packet->id << std::endl;
}

void ScnMgr::processLoginFail(char* message)
{
	std::cout << "Invalid Id!!" << std::endl;
}

void ScnMgr::processChat(char* message)
{

}

void ScnMgr::processStatChange(char* message)
{
	
}

float g_time = 0.f;

GLuint ScnMgr::getTextureId(const Object& object) const
{
	GLuint returnVal = 0;

	switch(object.type) {
	case ObjectType::Player:
		returnVal = mPlayerTexture;
		break;

	case ObjectType::FriendlyMonster:
		returnVal = mFriendlyMonsterTexture;
		break;

	case ObjectType::HostileMonster:
		returnVal = mHostileMonsterTexture;
		break;

	default:
		break;
	}

	return returnVal;
}

void ScnMgr::renderScene()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0f, 0.3f, 0.3f, 1.0f);

	if (prevTime == 0) {
		prevTime = timeGetTime();
		return;
	}

	renderBoard();
	renderObjects();
	
	g_time += 0.001;
}

void ScnMgr::renderBoard()
{
	std::scoped_lock<std::mutex> lock(mLock);
	WORD myX = MAP_WIDTH / 2;
	WORD myY = MAP_HEIGHT / 2;
	if (mMyId != 0) {
		myX = mObjects[mMyId]->xPos;
		myY = mObjects[mMyId]->yPos;
	}

	float r, g, b;
	for (int i = -WINDOW_YBLOCKS / 2; i < WINDOW_YBLOCKS / 2; ++i) {
		for (int j = -WINDOW_XBLOCKS / 2; j < WINDOW_XBLOCKS / 2; ++j) {
			// 색 번갈아가면서 그리기
			if ((myX + i + myY + j) % 2 == 0) {
				r = 0.3f;
				g = 0.7f;
				b = 0.2f;
			}
			else {
				r = 0.7f;
				g = 0.8f;
				b = 0.6f;
			}
			mRenderer->DrawSolidRect(
				(j / (float)WINDOW_XBLOCKS) * WINDOW_XBLOCKS * UNIT + (1.f / (float)WINDOW_XBLOCKS) * WINDOW_XBLOCKS * UNIT / 2.f, 
				(i / (float)WINDOW_YBLOCKS) * WINDOW_YBLOCKS * UNIT + (1.f / (float)WINDOW_YBLOCKS) * WINDOW_YBLOCKS * UNIT / 2.f, 
				0.f, 
				UNIT, 
				UNIT, 
				r, g, b, 1.f);
			//mRenderer->DrawSolidRect(0.f, 0.f, -1000.f, 490.f, 490.f, 1.f, 1.f, 1.f, 1.f);
		
		}
	}
}

void ScnMgr::renderObjects()
{
	std::scoped_lock<std::mutex> lock(mLock);
	// 시야에 들어온 애들만 그려줘야 한다...
	WORD myXPos = MAP_WIDTH / 2;
	WORD myYPos = MAP_HEIGHT / 2;
	if (mMyId != 0) {
		myXPos = mObjects[mMyId]->xPos;
		myYPos = mObjects[mMyId]->yPos;
	}

	for(auto object : mObjects) {
		WORD xPos = object.second->xPos;
		WORD yPos = object.second->yPos;
		int totalSeqX = 0;
		int totalSeqY = 0;

		if (object.second->type == ObjectType::Player ||
			object.second->type == ObjectType::FriendlyMonster ||
			object.second->type == ObjectType::HostileMonster) {
			totalSeqX = 1;
			totalSeqY = 4;
		}

		GLuint textureId = getTextureId(*object.second);

		// 화면에 나간 애들은 그리지 않는다. 시야 25 x 25
		if(xPos - myXPos < -VIEW_RANGE ||
		   xPos - myXPos > VIEW_RANGE || 
		   yPos - myYPos < -VIEW_RANGE ||
		   yPos - myYPos > VIEW_RANGE) {
			continue;
		}

		if (object.second->type == ObjectType::Player ||
			object.second->type == ObjectType::FriendlyMonster ||
			object.second->type == ObjectType::HostileMonster) {
			mRenderer->DrawTextureRectSeqXY(
				((xPos - myXPos) / (float)WINDOW_XBLOCKS) * WINDOW_XBLOCKS * UNIT + (1.f / (float)WINDOW_XBLOCKS) * WINDOW_XBLOCKS * UNIT / 2.f,
				((yPos - myYPos) / (float)WINDOW_YBLOCKS) * WINDOW_YBLOCKS * UNIT + (1.f / (float)WINDOW_YBLOCKS) * WINDOW_YBLOCKS * UNIT / 2.f,
				0.f,
				UNIT,
				UNIT,
				1.f, 1.f, 1.f, 1.f, textureId,
				0, static_cast<int>(object.second->dir), totalSeqX, totalSeqY
			);
		}
		// 이외의 애들은 이거로
		else {
			mRenderer->DrawTextureRect(
				((xPos - myXPos) / (float)WINDOW_XBLOCKS) * WINDOW_XBLOCKS * UNIT + (1.f / (float)WINDOW_XBLOCKS) * WINDOW_XBLOCKS * UNIT / 2.f,
				((yPos - myYPos) / (float)WINDOW_YBLOCKS) * WINDOW_YBLOCKS * UNIT + (1.f / (float)WINDOW_YBLOCKS) * WINDOW_YBLOCKS * UNIT / 2.f,
				0.f,
				UNIT,
				UNIT,
				1.f, 1.f, 1.f, 1.f, textureId
			);
		}
	}
}

ScnMgr::~ScnMgr()
{
	if (mRenderer) {
		delete mRenderer;
		mRenderer = NULL;
	}
}

float size = 0.5f;
float aclY = 0.f;

void ScnMgr::update(float elapsedTime)
{
	//TODO: 큐에 뭐 없을때는 안돌아야 함...
	while (true) {
		char* message = mNetworkManager.pop();

		if (message != nullptr) {
			switch (*reinterpret_cast<MessageType*>(&message[sizeof(MessageSize)])) {
			case MessageType::SCLoginOK:
				processLoginOK(message);
				break;

			case MessageType::SCAddObject:
				processAddObject(message);
				break;

			case MessageType::SCPositionInfo:
				processPositionInfo(message);
				break;

			case MessageType::SCRemoveObject:
				processRemoveObject(message);
				break;

			case MessageType::SCLoginFail:
				processLoginFail(message);
				break;

			case MessageType::SCChat:
				processChat(message);
				break;
				
			case MessageType::SCStatChange:
				processStatChange(message);
				break;

			default:
				std::cout << "Unknown packet." << std::endl;
				while (true);
			}
		}
		else {
			break;
		}

		if (message != nullptr) {
			delete[] message;
		}
	}
}

