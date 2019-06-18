#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();
	void DrawSolidRect(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a);
	void DrawSolidRectHeight(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, float height);
	void DrawTextureRect(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID);
	void DrawTextureRectDepth(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID, float depth);
	void DrawTextureRectHeight(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID, float height);
	void DrawTextureRectSeqXY(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID, int currSeqX, int currSeqY, int totalSeqX, int totalSeqY);
	void DrawTextureRectSeqXYDepth(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID, int currSeqX, int currSeqY, int totalSeqX, int totalSeqY, float depth);
	void DrawTextureRectSeqXYHeight(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID, int currSeqX, int currSeqY, int totalSeqX, int totalSeqY, float height);
	void DrawParticle(float x, float y, float z, float size, float r, float g, float b, float a, float gDirX, float gDirY, GLuint texID, float timeInSeconds);
	void DrawParticleClimate(float x, float y, float z, float size, float r, float g, float b, float a, float gDirX, float gDirY, GLuint texID, float ratio, float timeInSeconds);
	void DrawSolidRectGauge(float x, float y, float z, float sx, float sy, float r, float g, float b, float a, float height, float gauge);
	void DrawBorderXY(float x, float y, float z, float width, float height, float r, float g, float b, float a);

	void SetCameraCenterPos(float x, float y);

	GLuint CreatePngTexture(const char * filePath);
	GLuint CreateBmpTexture(const char * filePath);
	void DeleteTexture(GLuint texID);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(const char* filename, std::string *target);
	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void GetGLPosition(float x, float y, float *newX, float *newY);
	void GetGLShadow(float x, float *newV);
	float GetDepth(float y);
	unsigned char * loadBMPRaw(const char * imagepath, unsigned int& outWidth, unsigned int& outHeight, bool flipY);

	void CreateParticleVBO();
	void CreateParticleCloudVBO();

	bool m_Initialized = false;
	
	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	GLuint m_TexShadow = 0;

	GLuint m_VBORect = 0;
	GLuint m_VBOTexRect = 0;
	GLuint m_VBORectBorder = 0;
	GLuint m_VBOParticles = 0;
	GLuint m_VBOParticleCloud = 0;

	GLuint m_SolidRectShader = 0;
	GLuint m_TextureRectShader = 0;
	GLuint m_TextureRectSeqShader = 0;
	GLuint m_SolidRectGaugeShader = 0;
	GLuint m_SolidRectXYShader = 0;
	GLuint m_ParticleWithTextureShader = 0;
	
	int m_ParticleCount;
	int m_ParticleVertexCount;
	int m_ParticleCloudCount;
	int m_ParticleCloudVertexCount;

	float m_CameraCenterPosX = 0.f;
	float m_CameraCenterPosY = 0.f;
};

