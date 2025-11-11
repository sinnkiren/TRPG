#pragma once
#include "CommonTypes.h"
#include "renderer.h"
#include "CTexture.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CIndexBuffer.h"

class CSprite 
{
	static CShader m_Shader;

	CTexture m_Texture{};
	CMaterial m_Material{};

	int m_Width = 0;
	int m_Height = 0;

	CIndexBuffer				m_IndexBuffer;
	CVertexBuffer<VERTEX_3D>	m_VertexBuffer;

	std::vector<VERTEX_3D>		m_Vertices{};

public:
	CSprite(int width, int height, std::string texfilename,Vector2 uv[4]);
	CSprite(int width, int height, std::string texfilename,Vector2 uv[4],MATERIAL mtr);
	virtual ~CSprite() {
	}

	void Update();
	void Draw(Vector3 scale, Vector3 rotation, Vector3 pos);
	void Dispose();
	void ModifyUV(const Vector2* uv);
};