#ifndef NDEBUG
#include "BattleScene.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include "Logging.h"

// Added: Render Dev-only battle log window
void BattleScene::RenderBattleLogWindow()
{
    if (!g_SceneManager.IsDevMode() || !m_showBattleLog) return;
    if (ImGui::GetCurrentContext() == nullptr) return;

    ImGui::Begin("Battle Log", &m_showBattleLog, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Battle Log (Dev)");
    ImGui::Separator();
    ImGui::BeginChild("BattleLogWindow", ImVec2(400, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : logLines) {
        ImGui::TextWrapped("%s", line.c_str());
    }
    if (scrollLogToBottom) {
        ImGui::SetScrollHereY(1.0f);
        scrollLogToBottom = false;
    }
    ImGui::EndChild();
    ImGui::End();
}

// Embedded dev contents renderer for centralized Dev Panel
void BattleScene::RenderDevPanelContents()
{
    ImGui::Text("Battle Log (embedded)");
    ImGui::Separator();

    // --- Dev tools: enemy HP, turn control, force victory/defeat ---
    if (ImGui::CollapsingHeader("Battle Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        // Enemy list with HP controls
        ImGui::Text("Enemies (%d):", (int)enemies.size());
        for (int i = 0; i < (int)enemies.size(); ++i) {
            auto& e = enemies[i];
            ImGui::PushID(i);
            ImGui::BeginGroup();
            ImGui::Text("%d: %s", i, e.name.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Select")) selectedEnemy = i;
            ImGui::Separator();

            // HP editing
            int hp = e.hp;
            int maxHp = e.maxHp;
            ImGui::InputInt("HP", &hp, 1, 10);
            ImGui::SameLine();
            if (ImGui::Button("Apply HP")) {
                e.hp = std::clamp(hp, 0, maxHp);
                PushLog(std::string("Dev: set ") + e.name + " HP=" + std::to_string(e.hp), 2);
            }
            ImGui::InputInt("Max HP", &maxHp, 1, 10);
            ImGui::SameLine();
            if (ImGui::Button("Apply Max")) {
                e.maxHp = std::max(1, maxHp);
                e.hp = std::min(e.hp, e.maxHp);
                PushLog(std::string("Dev: set ") + e.name + " MaxHP=" + std::to_string(e.maxHp), 2);
            }

            // Quick actions
            if (ImGui::Button("Kill")) { e.hp = 0; PushLog(e.name + " was killed (dev)", 1); }
            ImGui::SameLine();
            if (ImGui::Button("Heal +1")) { e.hp = std::min(e.maxHp, e.hp + 1); PushLog(e.name + " healed +1 (dev)", 1); }
            ImGui::SameLine();
            if (ImGui::Button("Heal Full")) { e.hp = e.maxHp; PushLog(e.name + " healed full (dev)", 1); }

            ImGui::EndGroup();
            ImGui::PopID();
            ImGui::Separator();
        }

        // Battle control buttons
        ImGui::Spacing();
        ImGui::Text("Battle Controls:");
        if (ImGui::Button("Next Turn")) {
            // advance phase manually
            if (phase == Phase::PlayerTurn) phase = Phase::EnemyTurn;
            else if (phase == Phase::EnemyTurn) phase = Phase::PlayerTurn;
            else { /* leave Victory/Defeat as-is */ }
            PushLog(std::string("Dev: advanced to phase ") + (phase == Phase::PlayerTurn ? "PlayerTurn" : (phase == Phase::EnemyTurn ? "EnemyTurn" : (phase==Phase::Victory?"Victory":"Defeat"))), 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Force Victory")) { phase = Phase::Victory; PushLog("Dev: forced Victory", 1); }
        ImGui::SameLine();
        if (ImGui::Button("Force Defeat")) { phase = Phase::Defeat; PushLog("Dev: forced Defeat", 1); }

        ImGui::Unindent();
    }

    ImGui::BeginChild("BattleLogEmbedded", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : logLines) ImGui::TextWrapped("%s", line.c_str());
    if (scrollLogToBottom) { ImGui::SetScrollHereY(1.0f); scrollLogToBottom = false; }
    ImGui::EndChild();
}

#endif
