#include "SceneManager.h"
#include "TitleScene.h"     //タイトル
#include "TRPGSelectScene.h"//TRPGの選択
#include "ScenarioScene.h"  //シナリオの選択
#include "CharacterSelect.h"//キャラクター作成と選択功績点があれば強化
#include "StoryPlayer.h"    //ゲーム本編
#include "BattleScene.h"    //戦闘シーン（オプション）
#include "Result.h"         //リザルト
                            //物語の進行状況記録
#include "IScene.h"
#include <DirectXMath.h>

// ここに他のシーンヘッダーも追加していく


SceneManager g_SceneManager; // 実体定義はここだけ！

void SceneManager::Initialize() {
    currentType = SceneType::TITLE;

    // currentScene を生成
    currentScene = std::make_unique<TitleScene>();

    // 生成後は必ず初期化
    if (currentScene) {
        currentScene->Initialize();
    }
}


// SceneManager.cpp
void SceneManager::Update() {
    // 1. 入力処理
    HandleInput();

    if (currentScene)
        currentScene->Update();

    // ← フレーム末尾で安全に反映
    ApplyPendingChange();
}

void SceneManager::HandleInput() {
    bool spacePressedNow = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    // 押した瞬間だけ反応
    if (spacePressedNow && !spacePressedLastFrame) {
        // 遷移マップに現在のシーンがあれば次に進む
        auto it = nextSceneMap.find(currentType);
        if (it != nextSceneMap.end()) {
            ChangeScene(it->second);
        }
    }

    spacePressedLastFrame = spacePressedNow;
}



void SceneManager::Render() {
    // currentScene が nullptr じゃないか確認してから描画
    if (currentScene) {
        currentScene->Render();
    }
    else {
        // デバッグ用メッセージ
        OutputDebugString(L"警告: Render 呼び出し時 currentScene が nullptr です\n");
    }
}

void SceneManager::ChangeScene(SceneType next)
{
    pendingChange = true;
    pendingSceneType = next;
}

void SceneManager::ApplyPendingChange() {
    if (!pendingChange) return;
    pendingChange = false;


    // 既存シーンを破棄
    currentType = pendingSceneType;

    if (currentScene)
    {
        currentScene.reset();
    }

    // 新しいシーンを生成
    switch (pendingSceneType) {
    case SceneType::TITLE:
        currentScene = std::make_unique<TitleScene>();
        SetWindowTextW(m_hWnd, L"TRPG - タイトル");
        break;
    case SceneType::TRPG_SELECT:
        currentScene = std::make_unique<TRPGSelectScene>();
        SetWindowTextW(m_hWnd, L"TRPG - TRPG選択");
        break;
    case SceneType::SCENARIO_SELECT:
        currentScene = std::make_unique<ScenarioScene>();
        SetWindowTextW(m_hWnd, L"TRPG - シナリオ選択");
        break;
    case SceneType::CHARACTER_SELECT:
        currentScene = std::make_unique<CharcterScene>();
        SetWindowTextW(m_hWnd, L"TRPG - キャラクター選択");
        break;
    case SceneType::GAME_PLAY:
        currentScene = std::make_unique<StoryPlayer>();
        SetWindowTextW(m_hWnd, L"TRPG - 本編");
        break;
	case SceneType::BATTLE:
        currentScene = std::make_unique<BattleScene>();
        SetWindowTextW(m_hWnd, L"TRPG - バトル");
        break;
    case SceneType::RESULT:
        currentScene = std::make_unique<Result>();
        SetWindowTextW(m_hWnd, L"TRPG - リザルト");
        break;
    }

    // 生成したら必ず初期化
    if (currentScene) {
        currentScene->Initialize();
    }
    else {
        OutputDebugString(L"警告: ChangeScene で currentScene が nullptr です\n");
    }
}


SceneType SceneManager::GetCurrentScene() const {
    return currentType;
}

void SceneManager::Finalize()
{
    // 必要ならシーンのリソース解放などをここに記述
}