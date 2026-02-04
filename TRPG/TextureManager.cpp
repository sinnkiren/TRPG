#pragma once
#include "TextureManager.h"
#include "Application.h"
#include "system/stb_image.h"
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include "Logging.h"

namespace TextureManager
{
    // Direct3D11 デバイスとアセットルート
    static ID3D11Device* g_device = nullptr;
    static std::string g_assetRoot = "assets/";
    static std::unordered_map<std::string, ID3D11ShaderResourceView*> g_cache;

    // 初期化
    void Initialize(ID3D11Device* device, const std::string& assetRoot)
    {
        g_device = device;
        g_assetRoot = assetRoot;
        if (!g_assetRoot.empty() && g_assetRoot.back() != '/' && g_assetRoot.back() != '\\')
            g_assetRoot += "/";
    }

    // 終了処理
    void Shutdown()
    {
        for (auto& p : g_cache) {
            if (p.second) p.second->Release();
        }
        g_cache.clear();
        g_device = nullptr;
    }

    // ファイルパス解決
    // Accepts absolute paths as-is. For relative paths, try assetRoot first, then current_path.
    static std::filesystem::path ResolvePath(const std::string& rel)
    {
        std::filesystem::path req(rel);
        // If caller passed an absolute path, use it directly
        if (req.is_absolute()) return req;

        std::filesystem::path p1 = std::filesystem::path(g_assetRoot) / rel;
        if (std::filesystem::exists(p1)) return p1;

        std::filesystem::path p2 = std::filesystem::current_path() / rel;
        if (std::filesystem::exists(p2)) return p2;

        // Fallback: return asset-root based path (may not exist)
        return p1;
    }

    // テクスチャ読み込み
    ID3D11ShaderResourceView* LoadTexture(const std::string& relativePath)
    {
        if (!g_device) return nullptr;
        // Resolve path (accepts absolute). Use resolved absolute path string as cache key.
        auto p = ResolvePath(relativePath);
        std::string key = p.string();

        // Cache check
        auto it = g_cache.find(key);
        if (it != g_cache.end()) return it->second;

        if (!std::filesystem::exists(p)) {
            std::ostringstream o; o << "TextureManager: file not found: " << p.string();
            Log::Log(Log::Level::Warning, o.str());
            g_cache[key] = nullptr;
            return nullptr;
        }

        // Log when an absolute path is used
        if (std::filesystem::path(relativePath).is_absolute()) {
            std::ostringstream o; o << "TextureManager: loading absolute path: " << p.string();
            Log::Log(Log::Level::Info, o.str());
        }

        // stbi でロード
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load(p.string().c_str(), &w, &h, &channels, 4);
        if (!pixels || w <= 0 || h <= 0) {
            std::ostringstream o; o << "TextureManager: stbi_load failed: " << p.string();
            Log::Log(Log::Level::Error, o.str());
            if (pixels) stbi_image_free(pixels);
            g_cache[key] = nullptr;
            return nullptr;
        }

        // Texture2D 作成
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(w);
        desc.Height = static_cast<UINT>(h);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = pixels;
        initData.SysMemPitch = static_cast<UINT>(w * 4);

        ID3D11Texture2D* tex = nullptr;
        HRESULT hr = g_device->CreateTexture2D(&desc, &initData, &tex);
        stbi_image_free(pixels);

        if (FAILED(hr) || !tex) {
            std::ostringstream o; o << "TextureManager: CreateTexture2D failed 0x" << std::hex << (unsigned int)hr << " for " << p.string();
            Log::Log(Log::Level::Error, o.str());
            if (tex) tex->Release();
            g_cache[key] = nullptr;
            return nullptr;
        }

        // ShaderResourceView 作成
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        hr = g_device->CreateShaderResourceView(tex, &srvDesc, &srv);
        tex->Release();

        if (FAILED(hr) || !srv) {
            std::ostringstream o; o << "TextureManager: CreateSRV failed 0x" << std::hex << (unsigned int)hr << " for " << p.string();
            Log::Log(Log::Level::Error, o.str());
            if (srv) srv->Release();
            g_cache[key] = nullptr;
            return nullptr;
        }

        g_cache[key] = srv;

        std::ostringstream ok; ok << "TextureManager: loaded " << p.string();
        Log::Log(Log::Level::Info, ok.str());

        return srv;
    }

    // ImGui 用ラッパー
    ImTextureID GetImGuiTexture(const std::string& relativePath)
    {
        ID3D11ShaderResourceView* srv = LoadTexture(relativePath);
        return reinterpret_cast<ImTextureID>(srv);
    }

    ImTextureID GetImGuiTextureID(const std::string& relativePath) // 互換用
    {
        return GetImGuiTexture(relativePath);
    }

    // 個別解放
    void ReleaseTexture(const std::string& relativePath)
    {
        // Resolve and use same keying as LoadTexture
        auto p = ResolvePath(relativePath);
        std::string key = p.string();
        auto it = g_cache.find(key);
        if (it != g_cache.end()) {
            if (it->second) it->second->Release();
            g_cache.erase(it);
        }
    }
}
