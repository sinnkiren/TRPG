#include "ImGuiFontLoader.h"
#include "system/imgui/imgui.h"
#include <filesystem>
#include <iostream>
#include <windows.h>
#include <string>
#include <vector>

#ifdef IMGUI_IMPL_DIRECTX11
extern void ImGui_ImplDX11_CreateDeviceObjects();
#endif

namespace ImGuiFontLoader
{
    static void LogDebug(const char* msg) {
        OutputDebugStringA(msg);
    }

    static bool TryAddFont(const char* path, float size)
    {
        ImGuiIO& io = ImGui::GetIO();
        std::string msg = std::string("ImGuiFontLoader: trying to load font: ") + path + "\n";
        LogDebug(msg.c_str());

        ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, nullptr, io.Fonts->GetGlyphRangesJapanese());
        if (font == nullptr) {
            std::string err = std::string("ImGuiFontLoader: AddFontFromFileTTF failed for ") + path + "\n";
            LogDebug(err.c_str());
            return false;
        }

        // フォントをデフォルトに設定
        io.FontDefault = font;

        // フォントテクスチャ再生成（DX11バックエンドを使用しているなら）
    #ifdef IMGUI_IMPL_DIRECTX11
        ImGui_ImplDX11_CreateDeviceObjects();
    #endif

        std::string ok = std::string("ImGuiFontLoader: loaded and set default font: ") + path + "\n";
        LogDebug(ok.c_str());
        return true;
    }

    bool InitializeImGuiFonts(const std::string& preferredFontPath)
    {
        ImGuiIO& io = ImGui::GetIO();

        // preferred が指定されていれば優先して試す
        if (!preferredFontPath.empty()) {
            try {
                if (std::filesystem::exists(std::filesystem::path(preferredFontPath))) {
                    if (TryAddFont(preferredFontPath.c_str(), 16.0f)) {
                        std::string s = std::string("ImGuiFontLoader: Loaded preferred font: ") + preferredFontPath + "\n";
                        LogDebug(s.c_str());
                        return true;
                    }
                } else {
                    std::string s = "ImGuiFontLoader: preferred font not found: " + preferredFontPath + "\n";
                    LogDebug(s.c_str());
                }
            } catch (...) {
                LogDebug("ImGuiFontLoader: exception while checking preferredFontPath\n");
            }
        }

        // resources 内の候補を先に試す
        const std::vector<std::string> candidatePaths = {
            "resources/fonts/NotoSansJP-Regular.otf",
            "resources/fonts/NotoSansJP-Regular.ttf",
            "resources/fonts/NotoSansCJKjp-Regular.otf",
            // Windows システムフォントの候補（固定リストのみ、全スキャンは行わない）
            "C:/Windows/Fonts/meiryo.ttc",
            "C:/Windows/Fonts/Meiryo.ttf",
            "C:/Windows/Fonts/msgothic.ttc",
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/seguiemj.ttf"
        };

        for (const auto& p : candidatePaths) {
            try {
                if (std::filesystem::exists(std::filesystem::path(p))) {
                    if (TryAddFont(p.c_str(), 16.0f)) {
                        std::string s = std::string("ImGuiFontLoader: loaded font: ") + p + "\n";
                        LogDebug(s.c_str());
                        return true;
                    }
                } else {
                    std::string s = std::string("ImGuiFontLoader: not found: ") + p + "\n";
                    LogDebug(s.c_str());
                }
            } catch (...) {
                LogDebug("ImGuiFontLoader: exception while iterating candidatePaths\n");
            }
        }

        //以前は Windows/Fonts 全体を走査していましたが、ファイル数が多く非常に重いため廃止しました。
        // 必要ならユーザーが絶対パスを渡すか、resources/fonts にフォントを配置してください。
        LogDebug("ImGuiFontLoader: failed to load any Japanese font. Place a TTF/OTF in resources/fonts/ or call InitializeImGuiFonts(\"path\").\n");
        return false;
    }
}