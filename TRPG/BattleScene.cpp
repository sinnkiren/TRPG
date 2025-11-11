#include "BattleScene.h"
#include "Dice.h"
#include "FearEffects.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include <algorithm>

void BattleScene::Initialize()
{
    // SceneManager からプレイヤーデータを受け取る
    CharcterScene::CharcterDate& player = g_SceneManager.GetPlayer();

    // プレイヤーの初期耐久力を設定
    player.endurance = player.maxEndurance;

    // テスト用の敵を作成（シナリオから読み込む予定）
    enemies.clear();
    enemies.push_back({ "Eerie Shadow", 18, 12, 1, 3 });
    enemies.push_back({ "Unnamable Stirring", 16, 12, 2, 2 });

    selectedEnemy = enemies.empty() ? -1 : 0;
    phase = Phase::PlayerTurn;
    lastRoll = 0;
    timeAccum = 0.0f;

    // ダメージ／耐久力トラッキングの初期化
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
    if (damageFlashTimer >0.0f) {
        damageFlashTimer = std::max(0.0f, damageFlashTimer - dt);
    }
    if (persistentTimer >0.0f) {
        persistentTimer = std::max(0.0f, persistentTimer - dt);
        if (persistentTimer <=0.0f) {
            // 残痕の表示時間が切れたらクリア
            persistentStage =0;
        }
    }

    // 耐久力の変化を検出（ダメージを受けたか）
    if (prevEndurance != -1 && player.endurance < prevEndurance) {
        // 瞬間的なフラッシュを開始
        damageFlashTimer = damageFlashDuration;

        //失った割合に基づき段階を決定（4等分)
        if (player.maxEndurance >0) {
            float lostPercent = (float)(player.maxEndurance - player.endurance) / (float)player.maxEndurance; //0..1
            int stage = std::min(4, std::max(0, int(lostPercent *4.0f)));
            // ステージ1以上なら残痕を設定
            if (stage >=1) {
                persistentStage = stage;
                // ステージに応じて残痕の表示時間を設定
                persistentTimer =6.0f + stage *4.0f; //例: stage1->10s, stage4->22s
            }
        }
    }
    prevEndurance = player.endurance;

    // 全ての敵が倒されたか判定
    bool anyAlive = false;
    for (auto &e : enemies) if (e.hp >0) { anyAlive = true; break; }
    if (!anyAlive) phase = Phase::Victory;
    // プレイヤーの敗北判定（ここでは耐久力0で敗北）
    if (player.endurance <=0) phase = Phase::Defeat;
}

void BattleScene::Render()
{

    ImGui::Begin("Battle");

    ImGui::Text("Player: %s (%s)", player.name.c_str(), player.job.c_str());
    ImGui::Text("HP: %d / %d", player.endurance, player.maxEndurance);
    float enduranceRatio = (player.maxEndurance>0) ? float(player.endurance) / float(player.maxEndurance) :0.0f;
    ImGui::ProgressBar(enduranceRatio, ImVec2(-1,0), "HP");

    ImGui::Separator();

    ImGui::Text("Enemies:");
    for (int i =0; i < (int)enemies.size(); ++i) {
        auto &e = enemies[i];
        ImGui::PushID(i);
        if (e.hp >0) {
            ImGui::Text("%s HP: %d/%d", e.name.c_str(), e.hp, e.maxHp);
            if (ImGui::Selectable("Target", selectedEnemy == i)) selectedEnemy = i;
        } else {
            ImGui::TextDisabled("%s (defeated)", e.name.c_str());
        }
        ImGui::PopID();
    }

    ImGui::Separator();

    // フェーズごとの表示と操作
    if (phase == Phase::PlayerTurn) {
        ImGui::Text("Your Turn");
        if (selectedEnemy >=0 && selectedEnemy < (int)enemies.size() && enemies[selectedEnemy].hp >0) {
            if (ImGui::Button("Attack (POW check, d100)")) {
                lastRoll = Dice::RollDie(100);
                bool success = (lastRoll <= player.pow *5); //例: COC風の判定
                if (success) {
                    int dmg =4 + Dice::RollDie(3); //4 + d3 ダメージ
                    enemies[selectedEnemy].hp = std::max(0, enemies[selectedEnemy].hp - dmg);
                } else {
                    //失敗: 耐久力減少および敵ターンへ
                    player.ApplyEnduranceLoss(enemies[selectedEnemy].fearDamage);
                    phase = Phase::EnemyTurn;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Wait")) {
                phase = Phase::EnemyTurn;
            }
        } else {
            ImGui::TextDisabled("Please select a valid target");
        }
    }
    else if (phase == Phase::EnemyTurn) {
        ImGui::Text("Enemy's turn...");

        // 生きている敵を集める
        std::vector<Enemy*> aliveEnemies;
        for (auto& e : enemies)
            if (e.hp > 0) aliveEnemies.push_back(&e);

        if (!aliveEnemies.empty()) {
            // ランダムな敵1体が攻撃
            Enemy* attacker = aliveEnemies[Dice::RollDie((int)aliveEnemies.size()) - 1];
            int hit = Dice::RollDie(20);
            if (hit >= 6) {
                player.ApplyEnduranceLoss(attacker->atk);
            }
            else {
                player.ApplyEnduranceLoss(1);
            }
        }

        phase = Phase::PlayerTurn;
    } else if (phase == Phase::Victory) {
        ImGui::TextColored(ImVec4(0,1,0,1), "Victory!");
        if (ImGui::Button("To Results")) {
            if (RequestSceneChange) RequestSceneChange( /* RESULT_SCENE_ID */3 );
        }
    } else if (phase == Phase::Defeat) {
        ImGui::TextColored(ImVec4(1,0,0,1), "Defeat...");
        if (ImGui::Button("To Title")) {
            if (RequestSceneChange) RequestSceneChange( /* TITLE_SCENE_ID */0 );
        }
    }

    ImGui::Separator();
    ImGui::Text("Last roll: %d", lastRoll);

    ImGui::End();

    // ダメージオーバーレイ／ダメージ表現（耐久力に基づく）
    float intensity =1.0f - enduranceRatio; //0..1 の強度

    // 優先度: ダメージ受領時の瞬間フラッシュを優先
    if (damageFlashTimer >0.001f) {
        float t = damageFlashTimer / damageFlashDuration; //1 ->0
        // 開始時ほど強くなるように反転して扱う
        float flashAlpha = std::clamp(t,0.0f,1.0f);
        // フラッシュは強めに描画
        DrawFearOverlay(std::min(1.0f, intensity +0.6f * flashAlpha), ImGui::GetTime(),0);
    }
    else if (persistentStage >0 && intensity >0.001f) {
        // 残痕がある場合はその段階に応じたオーバーレイを描画
        DrawFearOverlay(intensity, ImGui::GetTime(), persistentStage);
    }
}