#include "StoryPlayer.h"
#include "system/imgui/imgui.h"
#include "FearEffects.h"

// Runtime node UI: display current node text and choices and handle selection
void StoryPlayer::UpdateNode()
{
    if (!m_usingNodeGraph) return;
    auto it = m_nodeMap.find(m_currentNodeId);
    if (it == m_nodeMap.end()) return;
    EventNode &node = it->second;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        if (m_bgTex) {
            bg->AddImage(m_bgTex, vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
        } else {
            ImU32 col = ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            bg->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
        }
        // Use the non-static DrawCharacters exposed by StoryPlayer_Editor.cpp
        DrawCharacters(m_characters, bg, vp);
    }

    ImVec2 shake = FearEffects::GetShakeOffset();
    FearEffects::RenderOverlay();

    ImVec2 basePos(10.0f, 600.0f);
    ImVec2 posWithShake(basePos.x + shake.x, basePos.y + shake.y);

    ImGui::SetNextWindowPos(posWithShake, ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_dialogSize, ImGuiCond_Always);

    ImFont* font = ImGui::GetFont();
    float prevScale = 1.0f;
    if (font) { prevScale = font->Scale; font->Scale = m_textScale; }

    ImGui::Begin("Dialog", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + m_dialogSize.x - 16.0f);
    ImGui::TextWrapped("%s", node.text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Separator();

    for (int i = 0; i < (int)node.choices.size(); ++i) {
        const Choice &c = node.choices[i];
        if (ImGui::Button(c.text.c_str())) {
            SelectChoice(i);
        }
    }
    ImGui::End();

    if (font) font->Scale = prevScale;
}
