#include "ImGuiHelpers.h"
#include <vector>
#include <cstring>
#include <cmath>
#include "ImGuiHelpers.h"

bool ImGui_InputTextMultiline_String(const char* label, std::string& s, size_t extra, const ImVec2& size, ImGuiInputTextFlags flags)
{
    size_t bufSize = s.size() + extra + 1;
    std::vector<char> buf;
    try {
        buf.resize(bufSize);
    } catch (...) {
        return false; // allocation failed
    }
    if (!s.empty()) memcpy(buf.data(), s.c_str(), s.size());
    buf[s.size()] = '\0';
    bool changed = ImGui::InputTextMultiline(label, buf.data(), buf.size(), size, flags);
    if (changed) s.assign(buf.data());
    return changed;
}

// Draw a textured quad rotated around its center.
// uv coords are specified per corner in order: tl, tr, br, bl.
void ImGui_AddImageQuadRotated(ImDrawList* dl, ImTextureID tex, const ImVec2& center, float size, float angle,
    const ImVec2& uv_tl, const ImVec2& uv_tr, const ImVec2& uv_br, const ImVec2& uv_bl,
    ImU32 col)
{
    if (!dl || tex == nullptr) return;
    float half = size * 0.5f;
    // corners in local space (tl, tr, br, bl)
    ImVec2 local[4] = { ImVec2(-half, -half), ImVec2(half, -half), ImVec2(half, half), ImVec2(-half, half) };
    float s = std::sin(angle), c = std::cos(angle);
    ImVec2 pts[4];
    for (int i = 0; i < 4; ++i) {
        float x = local[i].x, y = local[i].y;
        float rx = x * c - y * s;
        float ry = x * s + y * c;
        pts[i] = ImVec2(center.x + rx, center.y + ry);
    }
    dl->AddImageQuad(tex, pts[0], pts[1], pts[2], pts[3], uv_tl, uv_tr, uv_br, uv_bl, col);
}

bool ImGui_InputText_String(const char* label, std::string& s, size_t extra, ImGuiInputTextFlags flags)
{
    size_t bufSize = s.size() + extra + 1;
    std::vector<char> buf;
    try {
        buf.resize(bufSize);
    } catch (...) {
        return false;
    }
    if (!s.empty()) memcpy(buf.data(), s.c_str(), s.size());
    buf[s.size()] = '\0';
    bool changed = ImGui::InputText(label, buf.data(), buf.size(), flags);
    if (changed) s.assign(buf.data());
    return changed;
}
