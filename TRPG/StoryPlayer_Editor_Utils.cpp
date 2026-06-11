#include "StoryPlayer.h"
#include "system/imgui/imgui.h"
#include "system/json.hpp"

using json = nlohmann::json;

namespace {
// parse characters JSON
void ParseCharactersJsonImpl(const json &cj, std::vector<CharacterState> &out)
{
    out.clear();
    if (!cj.is_array()) return;
    for (auto &ce : cj) {
        if (!ce.is_object()) continue;
        CharacterState s;
        if (ce.contains("id") && ce["id"].is_string()) s.id = ce["id"].get<std::string>();
        if (ce.contains("image") && ce["image"].is_string()) s.imagePath = ce["image"].get<std::string>();
        if (ce.contains("expression") && ce["expression"].is_string()) s.expression = ce["expression"].get<std::string>();
        if (ce.contains("visible") && ce["visible"].is_boolean()) s.visible = ce["visible"].get<bool>();
        if (ce.contains("position") && ce["position"].is_object()) {
            auto &p = ce["position"];
            float x = 0.0f, y = 0.0f;
            if (p.contains("x") && (p["x"].is_number_float() || p["x"].is_number_integer())) x = p["x"].get<float>();
            if (p.contains("y") && (p["y"].is_number_float() || p["y"].is_number_integer())) y = p["y"].get<float>();
            s.position = ImVec2(x, y);
        }
        out.push_back(s);
    }
}

// draw characters placeholder
void DrawCharactersImpl(const std::vector<CharacterState>& chars, ImDrawList* bg, const ImGuiViewport* vp)
{
    if (!bg || !vp) return;
    for (const auto &c : chars) {
        if (!c.visible) continue;
        ImVec2 pos(vp->Pos.x + c.position.x * vp->Size.x, vp->Pos.y + c.position.y * vp->Size.y);
        ImU32 col = ImGui::GetColorU32(ImVec4(1.0f,1.0f,1.0f,1.0f));
        ImVec2 sz(120,220);
        ImVec2 a = pos;
        ImVec2 b = ImVec2(pos.x + sz.x, pos.y + sz.y);
        bg->AddRectFilled(a,b, ImGui::GetColorU32(ImVec4(0.06f,0.06f,0.06f,0.9f)), 6.0f);
        bg->AddRect(a,b, ImGui::GetColorU32(ImVec4(1,1,1,0.08f)), 6.0f);
        std::string label = c.id;
        if (!c.expression.empty()) label += std::string(" (") + c.expression + ")";
        bg->AddText(NULL, ImGui::GetFontSize(), ImVec2(a.x+6, a.y+6), col, label.c_str());
    }
}
} // namespace

// Expose functions for linkage expected by StoryPlayer.cpp
void ParseCharactersJson(const json &cj, std::vector<CharacterState> &out) { ParseCharactersJsonImpl(cj,out); }
void DrawCharacters(const std::vector<CharacterState>& chars, ImDrawList* bg, const ImGuiViewport* vp) { DrawCharactersImpl(chars,bg,vp); }
