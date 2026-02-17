#pragma once

#include "SceneType.h"
#include <memory>
#include <string>
#include <Windows.h>
#include "system/imgui/imgui.h"
#include "IScene.h"
#include <unordered_map>

// CharacterSelect's CharcterDate is used for storing player data
#include "CharacterSelect.h"

class IScene;

class SceneManager {
public:
    void Initialize();
    void Update();
    void Render();
    void Finalize();

    // シーン遷移管理
    void ChangeScene(SceneType Next);
    void ApplyPendingChange();
    SceneType GetCurrentScene() const;

    // ウィンドウハンドル設定（Application から呼ぶ想定）
    void SetWindowHandle(HWND hwnd) { m_hWnd = hwnd; }

    // プレイヤーデータ操作
    void SetPlayer(const CharcterScene::CharcterDate& p);
    CharcterScene::CharcterDate& GetPlayer();
    const CharcterScene::CharcterDate& GetPlayer() const;

    // Handle file dropped from OS (Dev mode only)
    void HandleFileDrop(const std::string& path);

    // Development mode flag: guard dev-only features (drag/drop, detailed logs, tools)
    // Default: enabled in debug builds, disabled in release builds.
    bool IsDevMode() const { return m_devMode; }
    void SetDevMode(bool v);
    bool IsPlayMode() const { return !m_devMode; }

    // In Release builds, Dev features are unavailable. Helper to query at compile-time.
    static constexpr bool DevFeaturesCompiledIn()
    {
#ifndef NDEBUG
        return true;
#else
        return false;
#endif
    }

    // Result outcome setter/getter (used to indicate victory vs defeat when showing Result scene)
    void SetLastResultVictory(bool v) { m_lastResultVictory = v; }
    bool WasLastResultVictory() const { return m_lastResultVictory; }

    // Query whether a scene change is pending (useful for scenes to ignore input when a dev-forced change is pending)
    bool HasPendingChange() const;

private:
    // フレーム処理の補助（SceneManager.cpp で実装）
    void HandleInput();

    std::unique_ptr<IScene> currentScene;    // 現在のシーンインスタンス
    SceneType currentType = SceneType::TITLE; // 現在のシーン種類
    HWND m_hWnd = nullptr;                    // ウィンドウハンドル
    bool spacePressedLastFrame = false;       // 前フレームのスペースキー状態

    // シーン遷移マップ
    std::unordered_map<SceneType, SceneType> nextSceneMap = {
        { SceneType::TITLE, SceneType::TRPG_SELECT },
        { SceneType::TRPG_SELECT, SceneType::SCENARIO_SELECT },
        { SceneType::SCENARIO_SELECT, SceneType::CHARACTER_SELECT },
        { SceneType::CHARACTER_SELECT, SceneType::GAME_PLAY },
        { SceneType::GAME_PLAY, SceneType::BATTLE },
        { SceneType::BATTLE, SceneType::RESULT },
        { SceneType::RESULT, SceneType::TITLE },
    };

    // SceneManager が所有するプレイヤーデータ（値で保持）
    CharcterScene::CharcterDate playerData;

    // Dev mode flag (actual default set at runtime in Initialize)
    bool m_devMode = false;
    // F12 toggle edge detector
    bool f12PressedLastFrame = false;
    // Edge detectors for function keys F1..F12 (index by key number), default false
    bool fKeyPressedLast[13] = {};

    // Update window title to reflect current scene and dev/play mode
    void UpdateWindowTitle();

    // ペンディング遷移フラグ
    bool pendingChange = false;
    SceneType pendingSceneType = SceneType::TITLE;
    bool m_lastResultVictory = false;
};

extern SceneManager g_SceneManager;