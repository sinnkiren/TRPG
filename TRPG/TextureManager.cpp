#include "TextureManager.h"
#include "Application.h"
#include "system/stb_image.h"
#include <filesystem>
#include <sstream>

namespace TextureManager
{
    static ID3D11Device* g_device = nullptr;
    static std::string g_assetRoot = "assets/";
    static std::unordered_map<std::string, ID3D11ShaderResourceView*> g_cache;

    void Initialize(ID3D11Device* device, const std::string& assetRoot)
    {
        g_device = device;
        g_assetRoot = assetRoot;
        if (!g_assetRoot.empty() && g_assetRoot.back() != '/' && g_assetRoot.back() != '\\') g_assetRoot += "/";
    }

    void Shutdown()
    {
        for (auto &p : g_cache) {
            if (p.second) p.second->Release();
        }
        g_cache.clear();
        g_device = nullptr;
    }

    static std::filesystem::path ResolvePath(const std::string& rel)
    {
        std::filesystem::path p1 = std::filesystem::path(g_assetRoot) / rel;
        if (std::filesystem::exists(p1)) return p1;
        // fallback: current_path + rel
        std::filesystem::path p2 = std::filesystem::current_path() / rel;
        if (std::filesystem::exists(p2)) return p2;
        // exe dir + rel
        // Application::GetHInstance not needed here; approximate with current_path
        return p1; // 最終的に p1 を返す（呼び出し側で存在確認）
    }

    ID3D11ShaderResourceView* LoadTexture(const std::string& relativePath)
    {
        if (!g_device) return nullptr;
        auto it = g_cache.find(relativePath);
        if (it != g_cache.end()) return it->second;

        auto p = ResolvePath(relativePath);
        if (!std::filesystem::exists(p)) {
            std::ostringstream o; o << "TextureManager: file not found: " << p.string() << "\n";
            OutputDebugStringA(o.str().c_str());
            g_cache[relativePath] = nullptr;
            return nullptr;
        }

        int w=0,h=0,channels=0;
        unsigned char* pixels = stbi_load(p.string().c_str(), &w, &h, &channels, 4);
        if (!pixels || w<=0 || h<=0) {
            std::ostringstream o; o << "TextureManager: stbi_load failed: " << p.string() << "\n";
            OutputDebugStringA(o.str().c_str());
            if (pixels) stbi_image_free(pixels);
            g_cache[relativePath] = nullptr;
            return nullptr;
        }

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = static_cast<UINT>(w);
        desc.Height = static_cast<UINT>(h);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA initData;
        ZeroMemory(&initData, sizeof(initData));
        initData.pSysMem = pixels;
        initData.SysMemPitch = static_cast<UINT>(w * 4);

        ID3D11Texture2D* tex = nullptr;
        HRESULT hr = g_device->CreateTexture2D(&desc, &initData, &tex);
        stbi_image_free(pixels);
        if (FAILED(hr) || !tex) {
            char buf[256];
            sprintf_s(buf, "TextureManager: CreateTexture2D failed 0x%08X for %s\n", (unsigned int)hr, p.string().c_str());
            OutputDebugStringA(buf);
            if (tex) tex->Release();
            g_cache[relativePath] = nullptr;
            return nullptr;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        ID3D11ShaderResourceView* srv = nullptr;
        hr = g_device->CreateShaderResourceView(tex, &srvDesc, &srv);
        tex->Release();
        if (FAILED(hr) || !srv) {
            char buf[256];
            sprintf_s(buf, "TextureManager: CreateSRV failed 0x%08X for %s\n", (unsigned int)hr, p.string().c_str());
            OutputDebugStringA(buf);
            if (srv) srv->Release();
            g_cache[relativePath] = nullptr;
            return nullptr;
        }

        g_cache[relativePath] = srv;
        std::ostringstream ok; ok << "TextureManager: loaded " << p.string() << "\n";
        OutputDebugStringA(ok.str().c_str());
        return srv;
    }

    ImTextureID GetImGuiTexture(const std::string& relativePath)
    {
        ID3D11ShaderResourceView* srv = LoadTexture(relativePath);
        return reinterpret_cast<ImTextureID>(srv);
    }

    void ReleaseTexture(const std::string& relativePath)
    {
        auto it = g_cache.find(relativePath);
        if (it != g_cache.end()) {
            if (it->second) { it->second->Release(); }
            g_cache.erase(it);
        }
    }
}