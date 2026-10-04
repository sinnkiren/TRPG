#pragma once
#include "TextureManager.h"
#include "Application.h"
#include "system/stb_image.h"
#include <fstream>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include "Logging.h"
#include "AssetManager.h"

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
    // Simple path resolver without std::filesystem.
    // Uses fixed asset root + relative path concatenation as a stable approach for student projects.
    // Resolve a relative texture path to an absolute/best-effort path using AssetManager.
    // Accepts names like "dark-tunnel2.jpg" or optionally prefixed with "texture/".
    static std::string ResolvePath(const std::string& rel)
    {
        // If caller passed a path already under "texture/", strip that prefix so AssetManager doesn't duplicate it.
        std::string r = rel;
        if (r.rfind("texture/", 0) == 0) r = r.substr(8);
        if (r.rfind("texture\\", 0) == 0) r = r.substr(8);
        return AssetManager::GetTexturePath(r);
    }

    // Simple absolute-path detector for Windows/Unix-ish paths.
    static bool IsAbsolutePath(const std::string& p)
    {
        if (p.empty()) return false;

        // Windowsドライブ C:\~
        if (p.size() >= 2 &&
            std::isalpha(static_cast<unsigned char>(p[0])) &&
            p[1] == ':')
            return true;

        // ルートパス / または \\ で始まる
        if (p[0] == '/' || p[0] == '\\')
            return true;

        return false;
    }

    // テクスチャ読み込み
    ID3D11ShaderResourceView* LoadTexture(const std::string& relativePath)
    {
        if (!g_device) return nullptr;
        // Resolve path. Use resolved path string as cache key.
        std::string p;
        if (IsAbsolutePath(relativePath)) p = relativePath;
        else p = ResolvePath(relativePath);
        std::string key = p;

        // Cache check
        auto it = g_cache.find(key);
        if (it != g_cache.end()) return it->second;

        // Check existence using std::ifstream (portable, avoids filesystem dependency)
        std::ifstream ifs(p, std::ios::binary);
        if (!ifs) {
            std::ostringstream o; o << "TextureManager: file not found: " << p;
            Log::Log(Log::Level::Warning, o.str());
            g_cache[key] = nullptr;
            return nullptr;
        }

        // Log when an absolute path is used
        if (IsAbsolutePath(relativePath)) {
            std::ostringstream o; o << "TextureManager: loading absolute path: " << p;
            Log::Log(Log::Level::Info, o.str());
        }

        // stbi でロード
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load(p.c_str(), &w, &h, &channels, 4);
        if (!pixels || w <= 0 || h <= 0) {
            std::ostringstream o; o << "TextureManager: stbi_load failed: " << p;
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
            std::ostringstream o; o << "TextureManager: CreateTexture2D failed 0x" << std::hex << (unsigned int)hr << " for " << p;
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
            std::ostringstream o; o << "TextureManager: CreateSRV failed 0x" << std::hex << (unsigned int)hr << " for " << p;
            Log::Log(Log::Level::Error, o.str());
            if (srv) srv->Release();
            g_cache[key] = nullptr;
            return nullptr;
        }

        g_cache[key] = srv;

        std::ostringstream ok; ok << "TextureManager: loaded " << p;
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
        std::string p = ResolvePath(relativePath);
        if (IsAbsolutePath(relativePath)) p = relativePath;

        auto it = g_cache.find(p);
        if (it != g_cache.end()) {
            if (it->second) it->second->Release();
            g_cache.erase(it);
        }
    }
}
