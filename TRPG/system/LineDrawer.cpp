#include	<iostream>

#include	"CommonTypes.h"
#include	"CMeshRenderer.h"
#include	"CMaterial.h"
#include	"CLineMesh.h"
#include    "CShader.h"

static CLineMesh g_mesh;
static CMeshRenderer g_renderer;
static CMaterial g_material;

static CShader g_shader;

void LineDrawerInit() 
{
	g_mesh.Init(Vector3(0, 0, 0), Vector3(0, 0, 1), 1);
	g_renderer.Init(g_mesh);

	MATERIAL mtrl;
	// マテリアル生成
	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = FALSE;

	g_material.Create(mtrl);

	// シェーダーの初期化
	g_shader.Create(
		"shader/unlitTextureVS.hlsl",			// 頂点シェーダー
		"shader/unlitTexturePS.hlsl",			// ピクセルシェーダー
		"shader/GeometryShader.hlsl"			// ジオメトリシェーダ
		);			

}

void LineDrawerDraw(
	float length,
	Vector3 start,
	Vector3 direction, 
	Color col)
{
	g_mesh.Clear();
	g_mesh.Init(start, direction, length);
	Matrix4x4 mtx = Matrix4x4::Identity;

	// 頂点バッファを更新
	g_renderer.Modify(g_mesh.GetVertices());

	Renderer::SetWorldMatrix(&mtx);
	g_material.SetDiffuse(col);
	g_material.Update();

	g_shader.SetGPU();
	g_material.SetGPU();
	g_renderer.Draw(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
}
