#pragma once
#include <string>
#include <unordered_map>
#include <d3d11.h>
#include "system/imgui/imgui.h"

namespace TextureManager {

    // Initialize
    void Initialize(ID3D11Device* device, const std::string& assetRoot = "assets/");

    // Shutdown
    void Shutdown();

    // Load texture
    ID3D11ShaderResourceView* LoadTexture(const std::string& relativePath);

    // ImGui 用
    ImTextureID GetImGuiTexture(const std::string& relativePath);
    ImTextureID GetImGuiTextureID(const std::string& relativePath);

    // Release
    void ReleaseTexture(const std::string& relativePath);
}
