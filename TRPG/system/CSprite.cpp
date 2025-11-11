#include	"CSprite.h"

CShader CSprite::m_Shader{};

CSprite::CSprite(int width, int height, std::string texfilename,Vector2 uv[4])
{
	m_Width = width;
	m_Height = height;

	// マテリアル生成
	MATERIAL	mtrl;
	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = TRUE;

	m_Material.Create(mtrl);

	// テクスチャロード
	bool sts = m_Texture.Load(texfilename);
	assert(sts == true);

	// シェーダオブジェクト生成
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

	// 頂点データ
	m_Vertices.clear();
	m_Vertices.resize(4);

	m_Vertices[0].Position = Vector3(-width / 2.0f, -height / 2.0f, 0);
	m_Vertices[1].Position = Vector3( width / 2.0f, -height / 2.0f, 0);
	m_Vertices[2].Position = Vector3(-width / 2.0f, height  / 2.0f, 0);
	m_Vertices[3].Position = Vector3( width / 2.0f,  height / 2.0f, 0);

	m_Vertices[0].Diffuse = Color(1, 1, 1, 1);
	m_Vertices[1].Diffuse = Color(1, 1, 1, 1);
	m_Vertices[2].Diffuse = Color(1, 1, 1, 1);
	m_Vertices[3].Diffuse = Color(1, 1, 1, 1);

	m_Vertices[0].TexCoord = uv[0];
	m_Vertices[1].TexCoord = uv[1];
	m_Vertices[2].TexCoord = uv[2];
	m_Vertices[3].TexCoord = uv[3];

	// 頂点バッファ生成
	m_VertexBuffer.Create(m_Vertices);

	// インデックスバッファ生成
	std::vector<unsigned int> indices;
	indices.resize(4);

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 3;

	// インデックスバッファ生成
	m_IndexBuffer.Create(indices);
}

CSprite::CSprite(int width, int height, std::string texfilename, Vector2 uv[4],MATERIAL mtrl)
{
	m_Width = width;
	m_Height = height;

	m_Material.Create(mtrl);

	// テクスチャロード
	bool sts = m_Texture.Load(texfilename);
	assert(sts == true);

	// シェーダオブジェクト生成
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

	// 頂点データ
	// 頂点データ
	m_Vertices.clear();
	m_Vertices.resize(4);

	m_Vertices[0].Position = Vector3(-width / 2.0f, -height / 2.0f, 0);
	m_Vertices[1].Position = Vector3(width / 2.0f, -height / 2.0f, 0);
	m_Vertices[2].Position = Vector3(-width / 2.0f, height / 2.0f, 0);
	m_Vertices[3].Position = Vector3(width / 2.0f, height / 2.0f, 0);

	m_Vertices[0].Diffuse = Color(1, 1, 1, 1);
	m_Vertices[1].Diffuse = Color(1, 1, 1, 1);
	m_Vertices[2].Diffuse = Color(1, 1, 1, 1);
	m_Vertices[3].Diffuse = Color(1, 1, 1, 1);

	m_Vertices[0].TexCoord = uv[0];
	m_Vertices[1].TexCoord = uv[1];
	m_Vertices[2].TexCoord = uv[2];
	m_Vertices[3].TexCoord = uv[3];

	// 頂点バッファ生成
	m_VertexBuffer.Create(m_Vertices);

	// インデックスバッファ生成
	std::vector<unsigned int> indices;
	indices.resize(4);

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 3;

	// インデックスバッファ生成
	m_IndexBuffer.Create(indices);
}

void CSprite::ModifyUV(const Vector2* uv) 
{
	// 頂点データ
	m_Vertices[0].TexCoord = uv[0];
	m_Vertices[1].TexCoord = uv[1];
	m_Vertices[2].TexCoord = uv[2];
	m_Vertices[3].TexCoord = uv[3];

	// 頂点バッファ生成
	m_VertexBuffer.Modify(m_Vertices);

}


void CSprite::Update() 
{

}

void CSprite::Draw(Vector3 scale,Vector3 rotation,Vector3 pos) 
{
	// SRT情報作成
	SRT	srt;

	srt.scale = scale;
	srt.rot = rotation;
	srt.pos = pos;

	Matrix4x4 worldmtx;

	worldmtx = srt.GetMatrix();

	Renderer::SetWorldViewProjection2D();	// 2D用のビュー行列をセット

	Renderer::SetWorldMatrix(&worldmtx);	// GPUにセット

	// 描画の処理
	ID3D11DeviceContext* devicecontext;
	devicecontext = Renderer::GetDeviceContext();

	// トポロジーをセット（旧プリミティブタイプ）
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();

	m_Material.SetGPU();
	m_Texture.SetGPU();

	devicecontext->DrawIndexed(
		4,							// 描画するインデックス数（四角形なんで４）
		0,							// 最初のインデックスバッファの位置
		0);

}

void CSprite::Dispose() 
{

}
