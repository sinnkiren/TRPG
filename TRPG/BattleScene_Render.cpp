#include "BattleScene.h"
#include "system/imgui/imgui.h"
#include "FearEffects.h"
#include "Dice.h"
#include "SceneManager.h"
#include "Logging.h"
#include "BattleLogic.h"
#include <algorithm>
#include <cmath>

namespace {
    template<typename T>
    constexpr T clamp_local(const T& v, const T& lo, const T& hi) noexcept {
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }
    template<typename T>
    constexpr T max_local(const T& a, const T& b) noexcept { return (a < b) ? b : a; }
    template<typename T>
    constexpr T min_local(const T& a, const T& b) noexcept { return (a < b) ? a : b; }
}

// Move Render implementation here to keep BattleScene.cpp focused
void BattleScene::Render()
{
    // ImGui が初期化されていなければ安全に早期リターン
    if (ImGui::GetCurrentContext() == nullptr) {
        if (g_SceneManager.IsDevMode()) ::Log::Log(::Log::Level::Warning, "BattleScene::Render skipped - ImGui context not initialized");
        return;
    }

    // フルスクリーン風に扱うメインウィンドウ
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 disp = io.DisplaySize;

    // Dev helper: if developer chose to hide the main Battle UI, skip creating full-screen window
    if (g_SceneManager.IsDevMode() && m_hideBattleUIInDev) {
        // Draw a tiny overlay control so developer can unhide the UI
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::SetNextWindowPos(ImVec2(disp.x - 12.0f, 12.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        if (ImGui::Begin("Battle Dev Overlay", nullptr, flags)) {
            ImGui::Text("Battle UI hidden (Dev)");
            if (ImGui::SmallButton("Show Battle UI")) {
                m_hideBattleUIInDev = false;
            }
            ImGui::Separator();
            if (ImGui::SmallButton("Toggle Battle Log")) {
#ifndef NDEBUG
                m_showBattleLog = !m_showBattleLog;
#endif
            }
        }
        ImGui::End();

        // still draw overlays so game feedback continues while UI hidden
        float intensity = 1.0f - ((player.maxEndurance > 0) ? float(player.endurance) / float(player.maxEndurance) : 0.0f);
        if (damageFlashTimer > 0.001f) {
            float t = damageFlashTimer / damageFlashDuration;
            float flashAlpha = clamp_local(t, 0.0f, 1.0f);
            DrawFearOverlay(min_local(1.0f, intensity + 0.6f * flashAlpha), ImGui::GetTime(), 0);
        }
        else if (persistentStage > 0 && intensity > 0.001f) {
            DrawFearOverlay(intensity, ImGui::GetTime(), persistentStage);
        }
        FearEffects::RenderOverlay();
        return;
    }
    ImGui::SetNextWindowSize(disp, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    // Make the window background transparent so the full-screen background image is visible
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    bool beginDraw = ImGui::Begin("Battle", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // Only push the global text color if we're actually going to draw the window contents.
    if (beginDraw) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
    }

    // If Begin() returned false, we must call End() and avoid submitting UI, but still pop the WindowBg we pushed.
    if (!beginDraw) {
        ImGui::End();
        ImGui::PopStyleColor(); // pop WindowBg
        return;
    }

    // 全体サイズと下部コマンド領域の高さ
    const float cmdHeight = 160.0f;
    // Responsive right panel width: scale with display width but clamp to reasonable range
    const float rightPanelWidth = clamp_local(disp.x * 0.22f, 220.0f, 340.0f);

    // 背景描画（画面全体に1枚絵を敷く）
    if (bgTexture) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (dl) {
            ImVec2 a(0, 0), b(disp.x, disp.y);
            // use full texture UVs
            dl->AddImage(bgTexture, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
        }
    }

    // --- 上段（敵領域 + 右側キャラステータス） ---
    ImGui::BeginChild("TopArea", ImVec2(0, -cmdHeight), false);

    // 左：敵表示領域（残り幅 - rightPanelWidth）
    float availW = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float enemiesW = max_local(0.0f, availW - rightPanelWidth - spacing - 200.0f);
    ImGui::BeginChild("EnemiesArea", ImVec2(enemiesW, 0), true);

    ImGui::Text("Enemies:");
    ImGui::Separator();

    // 大きめに敵一覧を表示（中央寄せの簡易表現）
    ImGui::Spacing();
    ImGui::Columns(1);
    ImGui::BeginGroup();

    // areaW は現在の利用可能幅を都度取得する（子ウィンドウ内の利用可能幅）
    for (int i = 0; i < (int)enemies.size(); ++i) {
        auto& e = enemies[i];
        ImGui::PushID(i);

        // 子要素毎に縦方向に並べる（中央揃え）
        float areaW = ImGui::GetContentRegionAvail().x;
        // 敵名
        {
            float textW = ImGui::CalcTextSize(e.name.c_str()).x;
            float curX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(curX + max_local(0.0f, (areaW - textW) * 0.5f));
            if (e.hp > 0) ImGui::Text("%s", e.name.c_str());
            else ImGui::TextDisabled("%s (defeated)", e.name.c_str());
        }

        // HP 行（新しい行で表示）
        {
            float hpTextW = ImGui::CalcTextSize("HP: 000/000").x; // おおよその幅
            float curX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(curX + max_local(0.0f, (areaW - hpTextW) * 0.5f));
            ImGui::Text("HP: %d/%d", e.hp, e.maxHp);
        }

        // ボタン（中央揃え）
        {
            const float btnW = 100.0f;
            float curX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(curX + max_local(0.0f, (areaW - btnW) * 0.5f));
            if (e.hp > 0) {
                if (ImGui::Button("Target", ImVec2(btnW, 0))) selectedEnemy = i;
            }
            else {
                // 敗北済みは選択不可だがボタンは無効表示にする
                ImGui::BeginDisabled();
                ImGui::Button("Target", ImVec2(btnW, 0));
                ImGui::EndDisabled();
            }
        }

        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndGroup();

    ImGui::EndChild(); // EnemiesArea

    ImGui::SameLine();

    // 右：キャラステータス領域（固定幅）
    ImGui::BeginChild("StatusArea", ImVec2(rightPanelWidth, 0), true);
    ImGui::Text("Player");
    ImGui::Separator();

    // player はクラスメンバ（SceneManager からコピーされる想定）
    ImGui::Text("%s (%s)", player.name.c_str(), player.job.c_str());
    ImGui::Separator();

    // HP 表示
    float enduranceRatio = (player.maxEndurance > 0) ? float(displayedEndurance) / float(player.maxEndurance) : 0.0f;
    ImGui::Text("HP");
    // If we have an atlas, draw the stylized bar from atlas and overlay a filled rect to represent the current value.
    if (uiAtlas) {
        // size of the bar we want on screen
        ImVec2 barSize(200, 16);
        ImVec2 barPos = ImGui::GetCursorScreenPos();

        // Choose UVs from analyzed atlas if available, otherwise fall back to hardcoded coordinates.
        ImVec2 red_uv0(0.02f, 0.50f), red_uv1(0.72f, 0.55f);
        if (atlasMap.valid) {
            red_uv0 = atlasMap.redBar.uv0;
            red_uv1 = atlasMap.redBar.uv1;
        }
        // Draw the bar background (atlas image). Using Image will advance the layout, so call it and then overlay.
        ImGui::Image(uiAtlas, barSize, red_uv0, red_uv1);

        // Overlay filled rect (use draw list so it stays on top of the image)
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (dl) {
            // Use a saturated red depending on missing HP
            ImVec4 tint(0.85f, 0.15f, 0.15f, 1.0f);
            ImU32 fillCol = ImGui::GetColorU32(tint);
            ImVec2 fillA(barPos.x + 2.0f, barPos.y + 2.0f);
            ImVec2 fillB(barPos.x + 2.0f + (barSize.x - 4.0f) * enduranceRatio, barPos.y + barSize.y - 2.0f);
            dl->AddRectFilled(fillA, fillB, fillCol);

            // Draw animated overlay (subtle gradient/shine)
            ImU32 shineCol = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
            float shineW = (barSize.x - 4.0f) * std::fmod(timeAccum * 0.2f, 1.0f);
            ImVec2 sA(fillA.x + shineW, fillA.y);
            ImVec2 sB(min_local(fillB.x, sA.x + 24.0f), fillB.y);
            if (sB.x > sA.x) dl->AddRectFilled(sA, sB, shineCol);
        }

        // Text on the right of the bar showing numeric value
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(barPos.x + barSize.x + 8.0f, barPos.y));
        ImGui::Text("%d / %d", player.endurance, player.maxEndurance);
        // advance cursor to below the image
        ImGui::SetCursorScreenPos(ImVec2(barPos.x, barPos.y + barSize.y + 6.0f));
    }
    else {
        ImGui::ProgressBar(enduranceRatio, ImVec2(-1, 0));
        ImGui::Text("%d / %d", player.endurance, player.maxEndurance);
    }

    ImGui::Separator();
    ImGui::Text("Stats");
    ImGui::Text("STR: %d  DEX: %d  POW: %d", player.str, player.dex, player.pow);
    ImGui::Text("INT: %d  SIZ: %d  CON: %d", player.int_, player.siz, player.con);

    ImGui::Separator();
    // デバッグ情報等
    ImGui::Text("Last roll: %d", lastRoll);

    // Atlas preview disabled per user request: no images drawn under "Last roll".
    // (HP bar and other UI still use the atlas where applicable.)

    ImGui::EndChild(); // StatusArea

    ImGui::EndChild(); // TopArea

    // --- 下段：戦闘コマンド領域 ---
    ImGui::BeginChild("CommandArea", ImVec2(0, cmdHeight), false);

    // Draw command area background from atlas (replace with the medium rectangle from the atlas).
    if (uiAtlas) {
        ImVec2 bgPos = ImGui::GetCursorScreenPos();
        ImVec2 bgAvail = ImGui::GetContentRegionAvail();
        // Use detected buttonBox region if available, otherwise fall back to a reasonable UV.
        ImVec2 cmd_uv0(0.62f, 0.02f), cmd_uv1(0.88f, 0.24f);
        if (atlasMap.valid) { cmd_uv0 = atlasMap.buttonBox.uv0; cmd_uv1 = atlasMap.buttonBox.uv1; }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (dl) {
            ImVec2 pmin = bgPos;
            ImVec2 pmax = ImVec2(bgPos.x + bgAvail.x, bgPos.y + bgAvail.y);
            dl->AddImage(uiAtlas, pmin, pmax, cmd_uv0, cmd_uv1, IM_COL32(255, 255, 255, 255));
        }
        // leave cursor where it is; subsequent UI will be drawn on top
    }

    ImGui::Separator();
    // Make command-area text more visible over textured background
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("Commands:");
    ImGui::Spacing();

    // コマンドボタン群（プレイヤーが行動できるかで無効化／敗北・勝利表示）
    if (phase == Phase::Defeat) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "You are defeated.");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::Text("戦闘は終了しています。リトライやメニューに戻る処理を追加してください。");
        ImGui::PopStyleColor();
    }
    else if (phase == Phase::Victory) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Victory!");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::Text("勝利しました。次の処理を追加してください。");
        ImGui::PopStyleColor();
    }
    else {
        // プレイヤーが実際に行動可能か判定（ターンがプレイヤーで、HP>0）
        bool canAct = (phase == Phase::PlayerTurn) && (player.endurance > 0);

        // 有効なターゲットが選択されているか（範囲チェックを必須化）
        auto isSelectedValid = [this]() -> bool {
            return (selectedEnemy >= 0 && selectedEnemy < (int)enemies.size() && enemies[selectedEnemy].hp > 0);
            };
        bool validTarget = isSelectedValid();

        if (validTarget) {
            Enemy* target = &enemies[selectedEnemy]; // 安全に参照
            ImGui::BeginDisabled(!canAct);
            // increase text contrast for buttons when on textured background
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
            if (ImGui::Button("Attack (POW check, d100)")) {
                // delegate to BattleLogic
                BattleLogic::PlayerAttack(*target, player, lastRoll, phase, [this](const std::string& s, int l){ PushLog(s, l); });
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            if (ImGui::Button("Wait")) {
                PushLog(player.name + " は行動を遅らせた。", 1);
                phase = Phase::EnemyTurn;
            }
            ImGui::EndDisabled();
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::Text("Please select a valid target");
            ImGui::PopStyleColor();
        }
    }

    ImGui::Separator();

    // 自動敵ターン表示（実際の敵行動は Update() で処理する）
    // NOTE: Do not run game-state changes from Render() - keep Render side-effect free.
    if (phase == Phase::EnemyTurn) {
        ImGui::Text("Enemy's turn...");
    }

    // --- Battle Log: moved to separate Dev-only window. Provide toggle button here when in Dev mode ---
    if (g_SceneManager.IsDevMode()) {
        ImGui::SameLine();
#ifndef NDEBUG
        if (ImGui::Button("Toggle Battle Log Window")) {
            m_showBattleLog = !m_showBattleLog;
        }
        ImGui::SameLine();
        if (ImGui::Button("Hide Battle UI")) {
            m_hideBattleUIInDev = true;
        }
#endif
    }

    if (beginDraw) {
        ImGui::PopStyleColor(); // restore text color pushed for Commands label
        ImGui::EndChild(); // CommandArea

        // --- ここで先に全体テキスト色の Push を戻す ---
        ImGui::PopStyleColor(); // restore global text color pushed after Begin()
    }

    ImGui::End(); // Battle window
    // Note: We popped the global text color only if beginDraw was true; now pop the WindowBg color we pushed at the top.
    ImGui::PopStyleColor(); // pop WindowBg

    // --- 画面エフェクト（フラッシュ／残痕） ---
    float intensity = 1.0f - ((player.maxEndurance > 0) ? float(player.endurance) / float(player.maxEndurance) : 0.0f);
    if (damageFlashTimer > 0.001f) {
        float t = damageFlashTimer / damageFlashDuration;
        float flashAlpha = clamp_local(t, 0.0f, 1.0f);
        DrawFearOverlay(min_local(1.0f, intensity + 0.6f * flashAlpha), ImGui::GetTime(), 0);
    }
    else if (persistentStage > 0 && intensity > 0.001f) {
        DrawFearOverlay(intensity, ImGui::GetTime(), persistentStage);
    }

    // Story 側や他から開始した FearEffects オーバーレイを描画
    FearEffects::RenderOverlay();

    // Dice visual overlay
    DiceVisual::Instance().Render();

}
