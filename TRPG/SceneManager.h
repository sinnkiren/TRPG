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

    // ペンディング遷移フラグ
    bool pendingChange = false;
    SceneType pendingSceneType = SceneType::TITLE;
};

extern SceneManager g_SceneManager;