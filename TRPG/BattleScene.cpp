#include "BattleScene.h"
#include "Dice.h"
#include "FearEffects.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <stdio.h>

void BattleScene::Initialize()
{
    // Use the BattleScene::player member (copied from SceneManager on scene change)
    // Ensure we don't reference g_SceneManager here.
    player.endurance = player.maxEndurance;

    // 敵の初期化
    enemies.clear();
    enemies.push_back({ "Eerie Shadow", 12, 12, 1, 3 });
    enemies.push_back({ "Unnamable Stirring", 12, 12, 2, 2 });

    selectedEnemy = enemies.empty() ? -1 : 0;
    phase = Phase::PlayerTurn;
    lastRoll = 0;
    timeAccum = 0.0f;

    // UI/演出用の初期化
    prevEndurance = player.endurance;
    damageFlashTimer = 0.0f;
    persistentStage = 0;
    persistentTimer = 0.0f;

    // ImGui ログ初期化（空）
    logLines.clear();
    // reserve して頻繁な再割当を防ぐ（maxLogLines はクラスメンバ想定）
    if (maxLogLines > 0) logLines.reserve(static_cast<size_t>(maxLogLines));
    scrollLogToBottom = false;

    // UI アトラス読み込み（TextureManager 経由）。assetRoot は TextureManager で設定している想定
    // ファイルは assets/texture/UIblok.png を想定しています。存在しない場合は nullptr のまま。
    uiAtlas = TextureManager::GetImGuiTexture("texture/UIblok.png");

    // 背景テクスチャ読み込み
    bgTexture = TextureManager::GetImGuiTexture("texture/dark-tunnel2.jpg");

    // Optional: automatically analyze atlas to get recommended UVs (used later)
    // This uses AtlasTools to heuristically split the atlas into regions.
    // If assets/texture/UIblok.png exists, the analysis will run and we may override hardcoded UVs.
    {
        AtlasTools::AtlasMap am = AtlasTools::AnalyzeAtlas("texture/UIblok.png");
        if (am.valid) {
            // store into members for use in Render()
            atlasMap = am;
        }
    }

    // Debug: report initialization and texture load status in Dev mode
    if (g_SceneManager.IsDevMode()) {
        char buf[512];
        sprintf_s(buf, sizeof(buf), "BattleScene::Initialize player=%s endurance=%d/%d uiAtlas=%p bgTexture=%p\n",
            player.name.c_str(), player.endurance, player.maxEndurance,
            static_cast<void*>(uiAtlas), static_cast<void*>(bgTexture));
        OutputDebugStringA(buf);
        if (!uiAtlas) OutputDebugStringA("BattleScene: warning - uiAtlas not loaded\n");
        if (!bgTexture) OutputDebugStringA("BattleScene: warning - bgTexture not loaded\n");
    }
}

void BattleScene::PushLog(const std::string& msg)
{
    // Echo logs to OutputDebugString in Dev mode for easier debugging
    if (g_SceneManager.IsDevMode()) {
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "[BattleLog] %s\n", msg.c_str());
        OutputDebugStringA(buf);
    }
    if (maxLogLines > 0) {
        while (logLines.size() >= maxLogLines) {
            logLines.erase(logLines.begin());
        }
    }

    logLines.push_back(msg);
    scrollLogToBottom = true;
}

void BattleScene::Update()
{
    // ImGui が初期化されていなければ安全に早期リターン
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    // オーバーレイ等で使う時間を加算
    float dt = ImGui::GetIO().DeltaTime;
    timeAccum += dt;

    // ダメージ閃光と残痕のタイマー処理
    if (damageFlashTimer > 0.0f) {
        damageFlashTimer = std::max(0.0f, damageFlashTimer - dt);
    }
    if (persistentTimer > 0.0f) {
        persistentTimer = std::max(0.0f, persistentTimer - dt);
        if (persistentTimer <= 0.0f) {
            // 残痕の表示時間が切れたらクリア
            persistentStage = 0;
        }
    }

    // 耐久力の変化を検出（ダメージを受けたか）
    if (prevEndurance != -1 && player.endurance < prevEndurance) {
        // 瞬間的なフラッシュを開始
        damageFlashTimer = damageFlashDuration;

        //失った割合に基づき段階を決定（4等分)
        if (player.maxEndurance > 0) {
            float lostPercent = (float)(player.maxEndurance - player.endurance) / (float)player.maxEndurance; //0..1
            int stage = std::min(4, std::max(0, int(lostPercent * 4.0f)));
            // ステージ1以上なら残痕を設定
            if (stage >= 1) {
                persistentStage = stage;
                // ステージに応じて残痕の表示時間を設定
                persistentTimer = 6.0f + stage * 4.0f; //例: stage1->10s, stage4->22s
            }
        }
    }
    prevEndurance = player.endurance;

    if (g_SceneManager.IsDevMode()) {
        char buf[256];
        sprintf_s(buf, sizeof(buf), "BattleScene::Update dt=%f timeAccum=%f player.endurance=%d\n", ImGui::GetIO().DeltaTime, timeAccum, player.endurance);
        OutputDebugStringA(buf);
    }

    // 全ての敵が倒されたか判定
    bool anyAlive = false;
    for (auto& e : enemies) if (e.hp > 0) { anyAlive = true; break; }
    if (!anyAlive) phase = Phase::Victory;
    // プレイヤーの敗北判定（ここでは耐久力0で敗北）
    if (player.endurance <= 0) phase = Phase::Defeat;
}

void BattleScene::Render()
{
    // ImGui が初期化されていなければ安全に早期リターン
    if (ImGui::GetCurrentContext() == nullptr) {
        if (g_SceneManager.IsDevMode()) OutputDebugStringA("BattleScene::Render skipped - ImGui context not initialized\n");
        return;
    }

    // フルスクリーン風に扱うメインウィンドウ
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    // Make the window background transparent so the full-screen background image is visible
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("Battle", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // --- ここで全体のテキスト色をより濃く設定して視認性を上げる ---
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));

    // 全体サイズと下部コマンド領域の高さ
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float cmdHeight = 160.0f;
    const float rightPanelWidth = 260.0f; // キャラステータスの幅

    // 背景描画（画面全体に1枚絵を敷く）
    if (bgTexture) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (dl) {
            ImVec2 dispSize = ImGui::GetIO().DisplaySize;
            ImVec2 a(0, 0), b(dispSize.x, dispSize.y);
            // use full texture UVs
            dl->AddImage(bgTexture, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
        }
    }

    // --- 上段（敵領域 + 右側キャラステータス） ---
    ImGui::BeginChild("TopArea", ImVec2(0, -cmdHeight), false);

    // 左：敵表示領域（残り幅 - rightPanelWidth）
    float availW = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float enemiesW = std::max(0.0f, availW - rightPanelWidth - spacing - 200.0f);
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
            ImGui::SetCursorPosX(curX + std::max(0.0f, (areaW - textW) * 0.5f));
            if (e.hp > 0) ImGui::Text("%s", e.name.c_str());
            else ImGui::TextDisabled("%s (defeated)", e.name.c_str());
        }

        // HP 行（新しい行で表示）
        {
            float hpTextW = ImGui::CalcTextSize("HP: 000/000").x; // おおよその幅
            float curX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(curX + std::max(0.0f, (areaW - hpTextW) * 0.5f));
            ImGui::Text("HP: %d/%d", e.hp, e.maxHp);
        }

        // ボタン（中央揃え）
        {
            const float btnW = 100.0f;
            float curX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(curX + std::max(0.0f, (areaW - btnW) * 0.5f));
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
    float enduranceRatio = (player.maxEndurance > 0) ? float(player.endurance) / float(player.maxEndurance) : 0.0f;
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
            ImU32 fillCol = ImGui::GetColorU32(ImVec4(0.85f, 0.15f, 0.15f, 1.0f)); // red tint
            ImVec2 fillA(barPos.x + 2.0f, barPos.y + 2.0f);
            ImVec2 fillB(barPos.x + 2.0f + (barSize.x - 4.0f) * enduranceRatio, barPos.y + barSize.y - 2.0f);
            dl->AddRectFilled(fillA, fillB, fillCol);
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
                lastRoll = Dice::RollDie(100);
                bool success = (lastRoll <= player.pow * 5);
                if (success) {
                    int dmg = 4 + Dice::RollDie(3); //4 + d3 ダメージ
                    target->hp = std::max(0, target->hp - dmg);
                    // ログ追加
                    PushLog(player.name + " は " + target->name + " に " + std::to_string(dmg) + " のダメージを与えた。");
                    if (target->hp == 0) {
                        PushLog(target->name + " を倒した！");
                    }
                }
                else {
                    //失敗: 耐久力減少および敵ターンへ
                    int dmg = target->fearDamage;
                    player.ApplyEnduranceLoss(dmg);
                    PushLog(player.name + " の攻撃は失敗した。耐久力が " + std::to_string(dmg) + " 減少した。");
                    phase = Phase::EnemyTurn;
                }
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            if (ImGui::Button("Wait")) {
                PushLog(player.name + " は行動を遅らせた。");
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

    // 自動敵ターン（デバッグ用の簡易実行）
    if (phase == Phase::EnemyTurn) {
        ImGui::Text("Enemy's turn...");
        // 生きている敵を集める
        std::vector<Enemy*> aliveEnemies;
        for (auto& e : enemies) if (e.hp > 0) aliveEnemies.push_back(&e);
        if (!aliveEnemies.empty()) {
            Enemy* attacker = aliveEnemies[Dice::RollDie((int)aliveEnemies.size()) - 1];
            int hit = Dice::RollDie(20);
            int dmg = (hit >= 6) ? attacker->atk : 1;
            player.ApplyEnduranceLoss(dmg);
            PushLog(attacker->name + " が攻撃し " + std::to_string(dmg) + " の耐久力を奪った。");
        }
        phase = Phase::PlayerTurn;
    }

    // --- ここでログ表示領域を追加（コマンド領域の下） ---
    ImGui::Separator();
    ImGui::Text("Battle Log:");
    ImGui::BeginChild("BattleLog", ImVec2(0, 80), true);
    for (const auto& line : logLines) {
        ImGui::TextWrapped("%s", line.c_str());
    }
    if (scrollLogToBottom) {
        ImGui::SetScrollHereY(1.0f);
        scrollLogToBottom = false;
    }
    ImGui::EndChild();

    ImGui::PopStyleColor(); // restore text color pushed for Commands label
    ImGui::EndChild(); // CommandArea

    // --- ここで先に全体テキスト色の Push を戻す ---
    ImGui::PopStyleColor(); // restore global text color pushed after Begin()

    ImGui::End(); // Battle window
    ImGui::PopStyleColor();

    // --- 画面エフェクト（フラッシュ／残痕） ---
    float intensity = 1.0f - ((player.maxEndurance > 0) ? float(player.endurance) / float(player.maxEndurance) : 0.0f);
    if (damageFlashTimer > 0.001f) {
        float t = damageFlashTimer / damageFlashDuration;
        float flashAlpha = std::clamp(t, 0.0f, 1.0f);
        DrawFearOverlay(std::min(1.0f, intensity + 0.6f * flashAlpha), ImGui::GetTime(), 0);
    }
    else if (persistentStage > 0 && intensity > 0.001f) {
        DrawFearOverlay(intensity, ImGui::GetTime(), persistentStage);
    }

    // Story 側や他から開始した FearEffects オーバーレイを描画
    FearEffects::RenderOverlay();
}