#include "SceneManager.h"
#include "TitleScene.h"
#include "TRPGSelectScene.h"
#include "ScenarioScene.h"
#include "CharacterSelect.h"
#include "StoryPlayer.h"
#include "BattleScene.h"
#include "Result.h"
#include "IScene.h"

// Windows ヘッダを先に読み込みます。
// WIN32_LEAN_AND_MEAN と NOMINMAX を定義して不要な定義・マクロ干渉を避ける。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// Direct3D を明示的にインクルード（d3d11.h の前に Windows.h が必要）
#include <d3d11.h>

// DirectXMath（DirectX 関連ヘッダは Windows.h の後に）
#include <DirectXMath.h>
#include <stdio.h>

SceneManager g_SceneManager; // 実体定義はここだけ！

void SceneManager::Initialize() {
    currentType = SceneType::TITLE;
    currentScene = std::make_unique<TitleScene>();
    if (currentScene) currentScene->Initialize();
}

void SceneManager::Update() {
    HandleInput();
    if (currentScene) currentScene->Update();
    ApplyPendingChange();
}

void SceneManager::HandleInput() {
    bool spacePressedNow = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    if (spacePressedNow && !spacePressedLastFrame) {
        auto it = nextSceneMap.find(currentType);
        if (it != nextSceneMap.end()) {
            ChangeScene(it->second);
        }
    }
    spacePressedLastFrame = spacePressedNow;
}

void SceneManager::SetPlayer(const CharcterScene::CharcterDate& p)
{
    // 値コピーして SceneManager が所有する
    playerData = p;

    // デバッグ出力（アドレスは playerData のアドレス）
    char buf[256];
    sprintf_s(buf, sizeof(buf), "SceneManager::SetPlayer called. playerData=%p name=%s\n",
        static_cast<void*>(&playerData),
        playerData.name.c_str());
    OutputDebugStringA(buf);
}

CharcterScene::CharcterDate& SceneManager::GetPlayer() { return playerData; }
const CharcterScene::CharcterDate& SceneManager::GetPlayer() const { return playerData; }

void SceneManager::Render() {
    if (currentScene) currentScene->Render();
    else OutputDebugString(L"警告: Render 呼び出し時 currentScene が nullptr です\n");
}

void SceneManager::ChangeScene(SceneType next)
{
    pendingChange = true;
    pendingSceneType = next;
}

void SceneManager::ApplyPendingChange() {
    if (!pendingChange) return;
    pendingChange = false;

    currentType = pendingSceneType;
    if (currentScene) currentScene.reset();

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
            // --- ApplyPendingChange() 内、BATTLE ケース直前にデバッグログを追加 ---
    case SceneType::BATTLE: {
            auto battle = std::make_unique<BattleScene>();

            // デバッグ: プレイヤーデータのアドレスと主要フィールドをログ出力
            {
                char buf[256];
                sprintf_s(buf, sizeof(buf),
                    "SceneManager: copying playerData -> BattleScene (addr playerData=%p name=%s endurance=%d/%d)\n",
                    static_cast<void*>(&playerData), playerData.name.c_str(), playerData.endurance, playerData.maxEndurance);
                OutputDebugStringA(buf);
            }

            // 値コピーで渡す
            battle->player = playerData;

            // デバッグ: コピー先のアドレス / 値を確認
            {
                char buf[256];
                sprintf_s(buf, sizeof(buf),
                    "SceneManager: after copy battle->player (name=%s endurance=%d/%d)\n",
                    battle->player.name.c_str(), battle->player.endurance, battle->player.maxEndurance);
                OutputDebugStringA(buf);
            }

            currentScene = std::move(battle);
            SetWindowTextW(m_hWnd, L"TRPG - バトル");
            break;
        }
    case SceneType::RESULT:
        currentScene = std::make_unique<Result>();
        SetWindowTextW(m_hWnd, L"TRPG - リザルト");
        break;
    }

    if (currentScene) currentScene->Initialize();
    else OutputDebugString(L"警告: ChangeScene で currentScene が nullptr です\n");
}

SceneType SceneManager::GetCurrentScene() const {
    return currentType;
}

void SceneManager::Finalize() {}