#include "BattleScene.h"
#include "Dice.h"
#include "FearEffects.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Logging.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdint>

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
    // 表示用耐久力を初期化（バーのアニメーション用）
    displayedEndurance = static_cast<float>(player.endurance);

    // ImGui ログ初期化（空）
    logLines.clear();
    // reserve して頻繁な再割当を防ぐ（maxLogLines はクラスメンバ想定）
    // deque does not support reserve; no-op
    scrollLogToBottom = false;

    // UI アトラス読み込み（TextureManager 経由）。assetRoot は TextureManager で設定している想定
    // ファイルは assets/texture/UIblok.png を想定しています。存在しない場合は nullptr のまま。
    // Load UI atlas and background texture with null checks
    uiAtlas = nullptr;
    try {
        uiAtlas = TextureManager::GetImGuiTexture("texture/UIblok.png");
    }
    catch (...) { uiAtlas = nullptr; }

    // 背景テクスチャ読み込み
    bgTexture = nullptr;
    try {
        bgTexture = TextureManager::GetImGuiTexture("texture/dark-tunnel2.jpg");
    }
    catch (...) { bgTexture = nullptr; }

    // Optional: automatically analyze atlas to get recommended UVs (used later)
    // This uses AtlasTools to heuristically split the atlas into regions.
    // If assets/texture/UIblok.png exists, the analysis will run and we may override hardcoded UVs.
    {
        try {
            AtlasTools::AtlasMap am = AtlasTools::AnalyzeAtlas("texture/UIblok.png");
            if (am.valid) {
                // store into members for use in Render()
                atlasMap = am;
            }
        }
        catch (const std::exception &ex) {
            if (g_SceneManager.IsDevMode()) {
                std::string s = std::string("BattleScene: AtlasTools::AnalyzeAtlas threw: ") + ex.what();
                ::Log::Log(::Log::Level::Debug, s);
            }
            // leave atlasMap invalid; Render will gracefully fallback
        }
    }

    // Debug: report initialization and texture load status in Dev mode
    if (g_SceneManager.IsDevMode()) {
        std::string s = "BattleScene::Initialize player=" + player.name
            + " endurance=" + std::to_string(player.endurance) + "/" + std::to_string(player.maxEndurance)
            + " uiAtlas=" + (uiAtlas ? std::to_string(reinterpret_cast<intptr_t>(static_cast<void*>(uiAtlas))) : std::string("(null)"))
            + " bgTexture=" + (bgTexture ? std::to_string(reinterpret_cast<intptr_t>(static_cast<void*>(bgTexture))) : std::string("(null)"))
            ;
        ::Log::Log(::Log::Level::Debug, s);
        if (!uiAtlas) ::Log::Log(::Log::Level::Warning, "BattleScene: warning - uiAtlas not loaded");
        if (!bgTexture) ::Log::Log(::Log::Level::Warning, "BattleScene: warning - bgTexture not loaded");
    }

    // Ensure DiceVisual singleton is reset (no-op if not used elsewhere)
    DiceVisual::Instance();
}

void BattleScene::PushLog(const std::string& msg, int level)
{
    // level: 0 = Error, 1 = Info, 2 = Debug (verbose)
    // Decide whether to emit to debug output based on dev mode and level
    // Route to central logging system and keep in local UI buffer
    if (level == 0) ::Log::Log(::Log::Level::Error, std::string("[BattleLog] ") + msg);
    else if (level == 1) ::Log::Log(::Log::Level::Info, std::string("[BattleLog] ") + msg);
    else ::Log::Log(::Log::Level::Debug, std::string("[BattleLog] ") + msg);

    // Maintain ring buffer of stored log lines (deque allows efficient pop_front)
#ifndef NDEBUG
    if (maxLogLines > 0) {
        while (logLines.size() >= maxLogLines) logLines.pop_front();
    }

    // Only store debug-level logs when in Dev mode
    if (g_SceneManager.IsDevMode() || level <= 1) {
        logLines.push_back(msg);
        scrollLogToBottom = true;
    }
#else
    // In Release builds, don't keep per-scene logs in memory; they are forwarded to central log only.
    (void)msg; (void)level;
#endif
}

// Update and Render implementations moved to separate files:
// - BattleScene_Update.cpp
// - BattleScene_Render.cpp

// Added: Render Dev-only battle log window
// Dev-only functions moved to BattleScene_Dev.cpp
