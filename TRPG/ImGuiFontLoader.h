#pragma once

#include <string>

#ifdef IMGUI_IMPL_DIRECTX11
// Match the actual signature in imgui_impl_dx11.cpp
extern bool ImGui_ImplDX11_CreateDeviceObjects();
#endif

namespace ImGuiFontLoader
{
    // 宣言のみ。実装は ImGuiFontLoader.cpp に置くこと。
    bool InitializeImGuiFonts(const std::string& preferredFontPath);
}