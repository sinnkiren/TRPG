#pragma once

#include <string>

#ifdef IMGUI_IMPL_DIRECTX11
// Match the actual signature in imgui_impl_dx11.cpp
extern bool ImGui_ImplDX11_CreateDeviceObjects();
#endif

namespace ImGuiFontLoader
{
    // éŒ¾‚Ì‚İBÀ‘•‚Í ImGuiFontLoader.cpp ‚É’u‚­‚±‚ÆB
    bool InitializeImGuiFonts(const std::string& preferredFontPath);
}