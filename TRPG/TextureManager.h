#pragma once
#include <string>
#include <unordered_map>
#include <d3d11.h>
#include "system/imgui/imgui.h"

namespace TextureManager {
	//
	void Initialize(ID3D11Device* device, const std::string& assetPRoot = "assets/texture");
	//
	void Shutdown();

	ID3D11ShaderResourceView* LoadTexture(const std::string& relativePath);
	//
	ImTextureID GetImGuiTextureID(const std::string& relativePath);
	//
	void Releasetexture(const std::string& relativePath);
}