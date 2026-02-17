#include "BattleScene.h"
#include "SceneManager.h"
#include "BattleLogic.h"
#include <chrono>
#include <algorithm>
#include "Logging.h"

// Move Update implementation here to keep BattleScene.cpp focused
void BattleScene::Update()
{
    // Use steady clock for Update delta-time to decouple from ImGui
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> delta = now - m_lastTick;
    m_lastTick = now;
    float dt = delta.count();
    // If dt is too large (e.g. paused or resumed), clamp to reasonable max
    if (dt > 0.5f) dt = 0.5f;
    timeAccum += dt;

    // 表示用耐久力を滑らかにプレイヤー耐久力へ追従させる（簡易イージング）
    {
        float target = static_cast<float>(player.endurance);
        float alpha = dt * 6.0f; // スムージング係数（調整可）
        if (alpha < 0.0f) alpha = 0.0f;
        else if (alpha > 1.0f) alpha = 1.0f;
        displayedEndurance += (target - displayedEndurance) * alpha;
    }


    // ダメージ閃光と残痕のタイマー処理
    if (damageFlashTimer > 0.0f) {
        damageFlashTimer = std::fmax(0.0f, damageFlashTimer - dt);
    }
    if (persistentTimer > 0.0f) {
        persistentTimer = std::fmax(0.0f, persistentTimer - dt);
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
            float lostPercent = static_cast<float>(player.maxEndurance - player.endurance) / static_cast<float>(player.maxEndurance); //0..1
            int tmp = static_cast<int>(lostPercent * 4.0f);
            if (tmp < 0) tmp = 0;
            if (tmp > 4) tmp = 4;
            int stage = tmp;
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
        std::string s = "BattleScene::Update dt=" + std::to_string(dt) + " timeAccum=" + std::to_string(timeAccum) + " player.endurance=" + std::to_string(player.endurance);
        ::Log::Log(::Log::Level::Debug, s);
    }

    // 全ての敵が倒されたか判定
    bool anyAlive = false;
    for (auto& e : enemies) if (e.hp > 0) { anyAlive = true; break; }
    if (!anyAlive) phase = Phase::Victory;
    // プレイヤーの敗北判定（ここでは耐久力0で敗北）
    if (player.endurance <= 0) phase = Phase::Defeat;

    // If we've reached an end phase, set result flag and request scene change to Result
    if (phase == Phase::Victory) {
        g_SceneManager.SetLastResultVictory(true);
        g_SceneManager.ChangeScene(SceneType::RESULT);
        return;
    }
    else if (phase == Phase::Defeat) {
        g_SceneManager.SetLastResultVictory(false);
        g_SceneManager.ChangeScene(SceneType::RESULT);
        return;
    }

    // Process enemy turn logic here (Update, not Render). This keeps game state changes frame-rate independent.
    if (phase == Phase::EnemyTurn) {
        BattleLogic::UpdateTurn(enemies, player, phase, [this](const std::string& s, int l){ PushLog(s, l); });
    }
}

// placeholder impl for symmetry
void BattleScene::UpdateImpl()
{
}
