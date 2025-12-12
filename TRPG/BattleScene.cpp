#include "BattleScene.h"
#include "Dice.h"
#include "FearEffects.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include <algorithm>

// --- 先頭付近の Initialize() を次のように修正 ---
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
}
void BattleScene::Update()
{
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

    // 全ての敵が倒されたか判定
    bool anyAlive = false;
    for (auto& e : enemies) if (e.hp > 0) { anyAlive = true; break; }
    if (!anyAlive) phase = Phase::Victory;
    // プレイヤーの敗北判定（ここでは耐久力0で敗北）
    if (player.endurance <= 0) phase = Phase::Defeat;
}

void BattleScene::Render()
{
    // フルスクリーン風に扱うメインウィンドウ
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::Begin("Battle", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // 全体サイズと下部コマンド領域の高さ
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float cmdHeight = 160.0f;
    const float rightPanelWidth = 260.0f; // キャラステータスの幅

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
    ImGui::ProgressBar(enduranceRatio, ImVec2(-1, 0));
    ImGui::Text("%d / %d", player.endurance, player.maxEndurance);

    ImGui::Separator();
    ImGui::Text("Stats");
    ImGui::Text("STR: %d  DEX: %d  POW: %d", player.str, player.dex, player.pow);
    ImGui::Text("INT: %d  SIZ: %d  CON: %d", player.int_, player.siz, player.con);

    ImGui::Separator();
    // デバッグ情報等
    ImGui::Text("Last roll: %d", lastRoll);

    ImGui::EndChild(); // StatusArea

    ImGui::EndChild(); // TopArea

    // --- 下段：戦闘コマンド領域 ---
    ImGui::BeginChild("CommandArea", ImVec2(0, cmdHeight), false);

    ImGui::Separator();
    ImGui::Text("Commands:");
    ImGui::Spacing();

    // コマンドボタン群（プレイヤーが行動できるかで無効化／敗北・勝利表示）
    if (phase == Phase::Defeat) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "You are defeated.");
        ImGui::TextDisabled("戦闘は終了しています。リトライやメニューに戻る処理を追加してください。");
    }
    else if (phase == Phase::Victory) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Victory!");
        ImGui::TextDisabled("勝利しました。次の処理を追加してください。");
    }
    else {
        // プレイヤーが実際に行動可能か判定（ターンがプレイヤーで、HP>0）
        bool canAct = (phase == Phase::PlayerTurn) && (player.endurance > 0);

        // 有効なターゲットが選択されているか
        bool validTarget = (selectedEnemy >= 0 && selectedEnemy < (int)enemies.size() && enemies[selectedEnemy].hp > 0);

        if (validTarget) {
            ImGui::BeginDisabled(!canAct);
            if (ImGui::Button("Attack (POW check, d100)")) {
                lastRoll = Dice::RollDie(100);
                bool success = (lastRoll <= player.pow * 5);
                if (success) {
                    int dmg = 4 + Dice::RollDie(3); //4 + d3 ダメージ
                    enemies[selectedEnemy].hp = std::max(0, enemies[selectedEnemy].hp - dmg);
                }
                else {
                    //失敗: 耐久力減少および敵ターンへ
                    player.ApplyEnduranceLoss(enemies[selectedEnemy].fearDamage);
                    phase = Phase::EnemyTurn;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Wait")) {
                phase = Phase::EnemyTurn;
            }
            ImGui::EndDisabled();
        }
        else {
            ImGui::TextDisabled("Please select a valid target");
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
            if (hit >= 6) player.ApplyEnduranceLoss(attacker->atk);
            else player.ApplyEnduranceLoss(1);
        }
        phase = Phase::PlayerTurn;
    }

    ImGui::EndChild(); // CommandArea

    ImGui::End(); // Battle window

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