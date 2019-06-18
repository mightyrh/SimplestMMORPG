#pragma once

#include "Renderer.h"
#include "Object.h"
#include "network_manager.h"

class ScnMgr
{
private:
	Renderer* mRenderer;
	NetworkManager mNetworkManager;

	std::map<int32_t, Object*> mObjects;

	// Textures
	GLuint mPlayerTexture = 0;
	GLuint mPlayerAttackTexture = 0;
	GLuint mFriendlyMonsterTexture = 0;
	GLuint mHostileMonsterTexture = 0;
	GLuint mAttackDownTexture = 0;
	GLuint mAttackUpTexture = 0;
	GLuint mAttackLeftTexture = 0;
	GLuint mAttackRightTexture = 0;
	GLuint mHealthBarTexture = 0;
	GLuint mPotionTexture = 0;


	int32_t mMyId;
	std::mutex mLock;

public:
	ScnMgr(int width, int height, const std::string& ip);
	~ScnMgr();
	Renderer* getRenderer() const { return mRenderer; }
	GLuint getTextureId(const Object& piece) const;

	void renderScene();
	void renderBoard();
	void renderObjects();

	void update(float eTime);

	void move(Dir move);

	// Process functions
	void processLoginOK(char* message);
	void processAddObject(char* message);
	void processPositionInfo(char* message);
	void processRemoveObject(char* message);
	void processLoginFail(char* message);
	void processChat(char* message);
	void processStatChange(char* message);
};


