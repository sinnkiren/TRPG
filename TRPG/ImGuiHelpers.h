#pragma once

#include "system/imgui/imgui.h"
#include <string>

// Safe wrapper for ImGui::InputTextMultiline that operates on std::string
// - 'extra' specifies additional capacity to allocate beyond current string size to allow editing
// - returns true if the string was modified
bool ImGui_InputTextMultiline_String(const char* label, std::string& s, size_t extra = 1024, const ImVec2& size = ImVec2(0,0), ImGuiInputTextFlags flags = 0);

// Convenience wrapper for single-line InputText (uses ImGui::InputText with same semantics)
bool ImGui_InputText_String(const char* label, std::string& s, size_t extra = 256, ImGuiInputTextFlags flags = 0);

// Draw a textured quad rotated around its center.
// uv coords are specified per corner in order: tl, tr, br, bl.
void ImGui_AddImageQuadRotated(ImDrawList* dl, ImTextureID tex, const ImVec2& center, float size, float angle,
                              const ImVec2& uv_tl, const ImVec2& uv_tr, const ImVec2& uv_br, const ImVec2& uv_bl,
                              ImU32 col = IM_COL32(255,255,255,255));
