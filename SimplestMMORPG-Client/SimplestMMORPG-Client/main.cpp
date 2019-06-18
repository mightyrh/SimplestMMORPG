/*
Copyright 2017 Lee Taek Hee (Korea Polytech University)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "ScnMgr.h"

ScnMgr* g_sceneMgr = nullptr;
DWORD g_prevTime = 0;
bool space = false;

void RenderScene(void)
{
	if (g_prevTime == 0) {
		g_prevTime = GetTickCount();
		return;
	}

	DWORD currTime = GetTickCount();
	DWORD elapsedTime = currTime - g_prevTime;
	g_prevTime = currTime;
	float eTime = (float)(elapsedTime / 1000.f);

	g_sceneMgr->update(eTime);
	g_sceneMgr->renderScene();

	glutSwapBuffers();
}

void Idle(void)
{
	RenderScene();
}

void MouseInput(int button, int state, int x, int y)
{
	RenderScene();
}

void KeyDownInput(unsigned char key, int x, int y)
{	

}

void keyUpInput(unsigned char key, int x, int y)
{

}

void SpecialKeyDownInput(int key, int x, int y)
{
	switch (key) {
	case GLUT_KEY_UP:
		g_sceneMgr->move(Dir::Up);
		break;

	case GLUT_KEY_DOWN:
		g_sceneMgr->move(Dir::Down);
		break;

	case GLUT_KEY_LEFT:
		g_sceneMgr->move(Dir::Left);
		break;

	case GLUT_KEY_RIGHT:
		g_sceneMgr->move(Dir::Right);
		break;

	//TODO: other special attacks
	}
}

void SpecialKeyUpInput(int key, int x, int y)
{
	RenderScene();
}


int main(int argc, char **argv)
{
	//setlocale(LC_ALL, "");
	//std::wcout.imbue(std::locale("korean"));

	std::string ip;
	std::cout << "Enter Ip: ";
	std::getline(std::cin, ip);

	// Initialize GL things
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WINDOW_XBLOCKS * UNIT, WINDOW_YBLOCKS * UNIT);
	glutCreateWindow("Game Server Lecture.");

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << " GLEW Version is 3.0\n ";
	}
	else
	{
		std::cout << "GLEW 3.0 not supported\n ";
	}

	// Initialize Renderer
	g_sceneMgr = new ScnMgr(WINDOW_XBLOCKS * UNIT, WINDOW_YBLOCKS * UNIT, ip);
	if(g_sceneMgr->getRenderer()->IsInitialized())
	{
		std::cout << "Renderer could not be initialized.. \n";
	}

	// callback 함수들
	// 각각 쓰레드를 가지고 돌아간다.
	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyDownInput);
	glutKeyboardUpFunc(keyUpInput);
	glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
	glutMouseFunc(MouseInput);
	glutSpecialFunc(SpecialKeyDownInput);
	glutSpecialUpFunc(SpecialKeyUpInput);

	// 리턴을 하지 않고 계속 랜더링을 한다
	// 키입력에 대비하는 등
	glutMainLoop();

	delete g_sceneMgr;

    return 0;
}

// 속도 = 이전속도 + 시간 * 가속도
// 위치 = 이전위치 + 시간 * 속도
// 단위를 1미터로 해보자