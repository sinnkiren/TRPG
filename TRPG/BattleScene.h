#pragma once
#include "IScene.h"
#include "CharacterSelect.h"
#include <vector>
#include <deque>
#include <functional>
#include <string>
#include "system/imgui/imgui.h"
#include "AtlasTools.h"
#include <chrono>

class BattleScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;
    // Dev-only UI/log: excluded from Release builds
#ifndef NDEBUG
    // Render Dev-only battle log window (toggled from in-scene UI)
    void RenderBattleLogWindow();
    // Render contents for centralized Dev panel (does not call ImGui::Begin/End)
    void RenderDevPanelContents();
#endif

    BattleScene() = default;

    // プレイヤー情報を受け取る（SceneManagerから呼ばれる）
    void SetPlayer(const CharacterScene::CharacterData& p) { player = p; prevEndurance = player.endurance; persistentStage = 0; persistentTimer = 0.0f; }

    // シーン切替要求ハンドラ（SceneManager にセットされる）
    std::function<void(int)> RequestSceneChange;

    CharacterScene::CharacterData player;
    // Public Enemy and Phase types so external BattleLogic can operate on them
    struct Enemy {
        std::string name;
        int hp = 10;
        int maxHp = 10;
        int atk = 3;
        int fearDamage = 2; // 敵が与える精神的ダメージ量
    };

    enum class Phase { PlayerTurn, EnemyTurn, Victory, Defeat } phase = Phase::PlayerTurn;

private:
    std::vector<Enemy> enemies;

    int selectedEnemy = -1;
    int lastRoll = 0;

    // UI / 時間管理
    float timeAccum = 0.0f;

    // 表示用耐久力（バーのアニメ用）
    float displayedEndurance = 0.0f;

    // ダメージ閃光・残痕管理
    int prevEndurance = -1; // 前フレームの耐久力（HP）
    float damageFlashTimer = 0.0f; // ダメージを受けた瞬間のフラッシュ用タイマー
    float damageFlashDuration = 0.35f; // フラッシュの持続時間（秒）

    int persistentStage = 0; // 傷の残り段階（0..4）
    float persistentTimer = 0.0f; // 傷が残る時間

    // ImGui 用の UI アトラステクスチャ
    ImTextureID uiAtlas = nullptr;
    AtlasTools::AtlasMap atlasMap;
    // 背景用テクスチャ
    ImTextureID bgTexture = nullptr;
    // Dev: allow hiding main Battle UI so Dev windows behind can be interacted with
    bool m_hideBattleUIInDev = false;

    // --- ログ表示用 ---
    // msg: text to append. level: 0=Error,1=Info,2=Debug (higher is more verbose)
    void PushLog(const std::string& msg, int level = 1);
#ifndef NDEBUG
    std::deque<std::string> logLines;
    size_t maxLogLines = 6;
    bool scrollLogToBottom = false;
    // Dev-only: toggle separate Battle Log window
    bool m_showBattleLog = false;
#endif

    // Timekeeping for Update() to avoid ImGui dependency
    std::chrono::steady_clock::time_point m_lastTick = std::chrono::steady_clock::now();
    // Separated implementations (to allow splitting into multiple translation units)
    void UpdateImpl();
    void RenderImpl();
};