#include "ImGuiFontLoader.h"
#include "system/imgui/imgui.h"
#include <filesystem>
#include <iostream>
#include <windows.h>

#ifdef IMGUI_IMPL_DIRECTX11
extern void ImGui_ImplDX11_CreateDeviceObjects();
#endif

namespace ImGuiFontLoader
{
    static void LogDebug(const char* msg) {
        OutputDebugStringA(msg);
    }

    static bool TryAddFont(const std::string& path, float size)
    {
        ImGuiIO& io = ImGui::GetIO();
        std::string msg = std::string("ImGuiFontLoader: trying to load font: ") + path + "\n";
        LogDebug(msg.c_str());

        ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, nullptr, io.Fonts->GetGlyphRangesJapanese());
        if (font == nullptr) {
            std::string err = std::string("ImGuiFontLoader: AddFontFromFileTTF failed for ") + path + "\n";
            LogDebug(err.c_str());
            return false;
        }

        // 読み込んだフォントをデフォルトに設定
        io.FontDefault = font;

    #ifdef IMGUI_IMPL_DIRECTX11
        ImGui_ImplDX11_CreateDeviceObjects();
    #endif

        std::string ok = std::string("ImGuiFontLoader: loaded and set default font: ") + path + "\n";
        LogDebug(ok.c_str());
        return true;
    }

    bool InitializeImGuiFonts(const std::string& preferredFontPath)
    {
        // 1) 優先パスを試す
        if (!preferredFontPath.empty()) {
            try {
                if (std::filesystem::exists(preferredFontPath)) {
                    if (TryAddFont(preferredFontPath, 16.0f)) {
                        MessageBoxA(nullptr, ("Loaded preferred font:\n" + preferredFontPath).c_str(), "Font Load", MB_OK);
                        return true;
                    }
                } else {
                    LogDebug(("ImGuiFontLoader: preferred font not found: " + preferredFontPath + "\n").c_str());
                }
            } catch (...) {
                LogDebug("ImGuiFontLoader: exception while checking preferredFontPath\n");
            }
        }

        // 2) resources/fonts フォルダ内の候補を試す（プロジェクト付属フォントがある場合）
        const std::vector<std::string> resourceCandidates = {
            "resources/fonts/NotoSansJP-Regular.otf",
            "resources/fonts/NotoSansJP-Regular.ttf",
            "resources/fonts/NotoSansCJKjp-Regular.otf"
        };
        for (const auto& p : resourceCandidates) {
            try {
                if (std::filesystem::exists(p)) {
                    if (TryAddFont(p, 16.0f)) {
                        MessageBoxA(nullptr, ("Loaded font:\n" + p).c_str(), "Font Load", MB_OK);
                        return true;
                    }
                } else {
                    LogDebug(("ImGuiFontLoader: resource not found: " + p + "\n").c_str());
                }
            } catch (...) {
                LogDebug("ImGuiFontLoader: exception while checking resourceCandidates\n");
            }
        }

        // 3) Windows の Fonts フォルダを自動スキャンして .ttf/.ttc/.otf を片っ端から試す
        const std::filesystem::path winFonts = "C:/Windows/Fonts";
        try {
            if (std::filesystem::exists(winFonts) && std::filesystem::is_directory(winFonts)) {
                for (auto& entry : std::filesystem::directory_iterator(winFonts)) {
                    try {
                        if (!entry.is_regular_file()) continue;
                        auto ext = entry.path().extension().string();
                        if (ext != ".ttf" && ext != ".TTF" && ext != ".ttc" && ext != ".TTC" && ext != ".otf" && ext != ".OTF") continue;

                        std::string candidate = entry.path().string();
                        if (TryAddFont(candidate, 16.0f)) {
                            MessageBoxA(nullptr, ("Loaded system font:\n" + candidate).c_str(), "Font Load", MB_OK);
                            return true;
                        }
                    } catch (...) {
                        // 個別ファイルの読み込みで失敗しても次へ
                    }
                }
            } else {
                LogDebug("ImGuiFontLoader: C:/Windows/Fonts not found or not a directory\n");
            }
        } catch (const std::exception& e) {
            std::string err = std::string("ImGuiFontLoader: exception while scanning Windows fonts: ") + e.what() + "\n";
            LogDebug(err.c_str());
        }

        MessageBoxA(nullptr, "ImGuiFontLoader: failed to load any Japanese font.\nPlace a TTF/OTF in resources/fonts/ or pass absolute path.", "Font Load Failed", MB_OK | MB_ICONERROR);
        LogDebug("ImGuiFontLoader: failed to load any Japanese font.\n");
        return false;
    }
}