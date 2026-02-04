#include "SceneManager.h"
#include "TitleScene.h"
#include "TRPGSelectScene.h"
#include "ScenarioScene.h"
#include "CharacterSelect.h"
#include "StoryPlayer.h"
#include "BattleScene.h"
#include "Result.h"
#include "IScene.h"
#include "Logging.h"

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
#include <string>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <set>

SceneManager g_SceneManager; // 実体定義はここだけ！

// Set default dev mode based on build type
static bool s_defaultDevMode =
#ifdef NDEBUG
    false;
#else
    true;
#endif

void SceneManager::Initialize() {
    currentType = SceneType::TITLE;
    currentScene = std::make_unique<TitleScene>();
    if (currentScene) currentScene->Initialize();
    // Set dev mode default (enabled in debug builds, disabled in release builds)
    m_devMode = s_defaultDevMode;
    // Update window title to reflect initial mode
    UpdateWindowTitle();
}

void SceneManager::SetDevMode(bool v)
{
    m_devMode = v;
    // Mirror to logging subsystem
    Log::SetDevMode(v);
    UpdateWindowTitle();
}

void SceneManager::HandleFileDrop(const std::string& path)
{
    // Decide behavior based on file extension
    try {
        std::filesystem::path p(path);
        // Resolve non-absolute paths similar to TextureManager: try assetRoot/current_path
        if (!p.is_absolute()) {
            std::filesystem::path alt = std::filesystem::path(std::filesystem::current_path()) / p;
            if (std::filesystem::exists(alt)) p = alt;
        }

        if (!std::filesystem::exists(p)) {
            ::Log::Log(::Log::Level::Warning, std::string("SceneManager::HandleFileDrop - dropped file does not exist: ") + p.string());
            return;
        }

        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        // JSON files: allowed in Play mode (GAME_PLAY) so story data can be loaded at runtime
        if (ext == ".json") {
            if (currentType == SceneType::GAME_PLAY) {
                StoryPlayer* sp = dynamic_cast<StoryPlayer*>(currentScene.get());
                if (sp) {
                    bool ok = sp->LoadFromFile(p.string());
                    if (ok) ::Log::Log(::Log::Level::Info, std::string("SceneManager: story JSON loaded from file drop: ") + p.string());
                    else ::Log::Log(::Log::Level::Warning, std::string("SceneManager: failed to load story JSON from file drop: ") + p.string());
                }
                else {
                    ::Log::Log(::Log::Level::Warning, "SceneManager: JSON dropped but current scene is not StoryPlayer");
                }
            }
            else {
                ::Log::Log(::Log::Level::Info, "SceneManager: JSON drop ignored in this scene (not GAME_PLAY)");
            }
            return;
        }

        // Image files: only in Dev mode
        static const std::set<std::string> imgExts = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif" };
        if (imgExts.find(ext) != imgExts.end()) {
            if (!m_devMode) {
                ::Log::Log(::Log::Level::Info, "SceneManager: image file drop ignored in Play mode");
                return;
            }

            if (currentType == SceneType::CHARACTER_SELECT) {
                CharcterScene* cs = dynamic_cast<CharcterScene*>(currentScene.get());
                if (!cs) return;
                std::string out = std::filesystem::path(p).string();
                // ↓ 変換
                std::string outStr(out.begin(), out.end());
                cs->SetPortraitPath(outStr);
                return;
            }
            else {
                // For other scenes, just log and ignore (could be extended later)
                ::Log::Log(::Log::Level::Info, "SceneManager: image drop received but not handled by current scene");
                return;
            }
        }

        // Unsupported file type
        ::Log::Log(::Log::Level::Warning, "SceneManager: dropped file has unsupported extension");
    }
    catch (const std::exception& ex) {
        std::string s = std::string("SceneManager::HandleFileDrop - exception: ") + ex.what();
        ::Log::Log(::Log::Level::Error, s);
    }
    catch (...) {
        ::Log::Log(::Log::Level::Error, "SceneManager::HandleFileDrop - unknown exception");
    }
}

void SceneManager::UpdateWindowTitle()
{
    if (!m_hWnd) return;
    // base title depending on currentType
    const wchar_t* base = L"TRPG";
    switch (currentType) {
    case SceneType::TITLE: base = L"TRPG - タイトル"; break;
    case SceneType::TRPG_SELECT: base = L"TRPG - TRPG選択"; break;
    case SceneType::SCENARIO_SELECT: base = L"TRPG - シナリオ選択"; break;
    case SceneType::CHARACTER_SELECT: base = L"TRPG - キャラクター選択"; break;
    case SceneType::GAME_PLAY: base = L"TRPG - 本編"; break;
    case SceneType::BATTLE: base = L"TRPG - バトル"; break;
    case SceneType::RESULT: base = L"TRPG - リザルト"; break;
    }

    std::wstring title = base;
    if (m_devMode) title += L" [DEV]";
    SetWindowTextW(m_hWnd, title.c_str());
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

    // F12 toggles Dev/Play mode (edge detect)
    bool f12PressedNow = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    // Only allow toggling Dev mode in debug builds
#ifndef NDEBUG
    if (f12PressedNow && !f12PressedLastFrame) {
        m_devMode = !m_devMode;
        Log::SetDevMode(m_devMode);
        if (m_devMode) Log::Log(Log::Level::Info, "SceneManager: Dev mode ON");
        else Log::Log(Log::Level::Info, "SceneManager: Dev mode OFF");
        UpdateWindowTitle();
    }
#endif
    f12PressedLastFrame = f12PressedNow;
}

void SceneManager::SetPlayer(const CharcterScene::CharcterDate& p)
{
    // 値コピーして SceneManager が所有する
    playerData = p;

    // デバッグ出力（アドレスは playerData のアドレス）
    if (m_devMode) {
        std::string s = "SceneManager::SetPlayer called. playerData=" + std::to_string(reinterpret_cast<intptr_t>(static_cast<void*>(&playerData))) + " name=" + playerData.name + "\n";
        ::Log::Log(::Log::Level::Debug, s);
    }
}

CharcterScene::CharcterDate& SceneManager::GetPlayer() { return playerData; }
const CharcterScene::CharcterDate& SceneManager::GetPlayer() const { return playerData; }

void SceneManager::Render() {
    if (currentScene) {
        // Separate main rendering from UI/ImGui rendering
        currentScene->Render();
    }
    else ::Log::Log(::Log::Level::Warning, "SceneManager::Render called with currentScene == nullptr");
    // After main scene rendering, render per-scene UI (ImGui) and global Dev windows
    if (currentScene) currentScene->RenderUI();

    // Dev-only: show central log window and Dev Panel (only in debug builds)
#ifndef NDEBUG
    if (m_devMode) {
        // Log window (separate, uses Logging::RenderImGui)
        static bool s_logOpen = true;

        // Dev Panel: three conceptual sections: Global / Scene / Log
        ImGui::Begin("Dev Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        // --- Global section ---
        if (ImGui::CollapsingHeader("Global", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::Text("Frame time: %.3f ms", io.DeltaTime * 1000.0f);
            ImGui::Text("Scene: %s", [&]() -> const char* {
                switch (currentType) {
                case SceneType::TITLE: return "Title";
                case SceneType::TRPG_SELECT: return "TRPG Select";
                case SceneType::SCENARIO_SELECT: return "Scenario Select";
                case SceneType::CHARACTER_SELECT: return "Character Select";
                case SceneType::GAME_PLAY: return "Story Player";
                case SceneType::BATTLE: return "Battle";
                case SceneType::RESULT: return "Result";
                default: return "Unknown";
                }
            }());
            ImGui::Unindent();
        }

        // --- Scene section ---
        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Provide an embedded area for the current scene to show its Dev contents
            ImGui::BeginChild("SceneDevArea", ImVec2(400, 200), true);
            if (currentScene) currentScene->RenderDevPanelContents();
            ImGui::EndChild();
        }

        // --- Log section ---
        if (ImGui::CollapsingHeader("Log", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Log window is available as a separate window.");
            ImGui::SameLine();
            if (ImGui::Button(s_logOpen ? "Hide Log" : "Show Log")) s_logOpen = !s_logOpen;
        }

        ImGui::End();

        // Render central log window (separate window so it can be resized/moved independently)
        if (s_logOpen) Log::RenderImGui("Log", &s_logOpen);
    }
#endif
}

void SceneManager::ChangeScene(SceneType next)
{
    pendingChange = true;
    pendingSceneType = next;
}

void SceneManager::ApplyPendingChange() {
    if (!pendingChange) return;
    pendingChange = false;
    // Debug: log pending change
    if (m_devMode) {
        std::string dbg = std::string("SceneManager: ApplyPendingChange -> pendingSceneType=") + std::to_string((int)pendingSceneType);
        ::Log::Log(::Log::Level::Debug, dbg);
    }
    currentType = pendingSceneType;
    if (currentScene) currentScene.reset();

    switch (pendingSceneType) {
    case SceneType::TITLE:
        currentScene = std::make_unique<TitleScene>();
        if (m_hWnd) SetWindowTextW(m_hWnd, L"TRPG - タイトル");
        break;
    case SceneType::TRPG_SELECT:
        currentScene = std::make_unique<TRPGSelectScene>();
        if (m_hWnd) SetWindowTextW(m_hWnd, L"TRPG - TRPG選択");
        break;
    case SceneType::SCENARIO_SELECT:
        currentScene = std::make_unique<ScenarioScene>();
        if (m_hWnd) SetWindowTextW(m_hWnd, L"TRPG - シナリオ選択");
        break;
    case SceneType::CHARACTER_SELECT:
        currentScene = std::make_unique<CharcterScene>();
        if (m_hWnd) SetWindowTextW(m_hWnd, L"TRPG - キャラクター選択");
        break;
    case SceneType::GAME_PLAY:
        currentScene = std::make_unique<StoryPlayer>();
        if (m_hWnd) SetWindowTextW(m_hWnd, L"TRPG - 本編");
        break;
            // --- ApplyPendingChange() 内、BATTLE ケース直前にデバッグログを追加 ---
    case SceneType::BATTLE: {
            auto battle = std::make_unique<BattleScene>();

            // デバッグ: プレイヤーデータのアドレスと主要フィールドをログ出力
            if (m_devMode) {
                std::string s = std::string("SceneManager: copying playerData -> BattleScene (addr=") + std::to_string(reinterpret_cast<intptr_t>(static_cast<void*>(&playerData)))
                    + " name=" + playerData.name + " endurance=" + std::to_string(playerData.endurance) + "/" + std::to_string(playerData.maxEndurance);
                ::Log::Log(::Log::Level::Debug, s);
            }

            // 値コピーで渡す
            battle->player = playerData;

            // デバッグ: コピー先のアドレス / 値を確認
            if (m_devMode) {
                std::string s = std::string("SceneManager: after copy battle->player (name=") + battle->player.name + " endurance=" + std::to_string(battle->player.endurance) + "/" + std::to_string(battle->player.maxEndurance);
                ::Log::Log(::Log::Level::Debug, s);
            }

            currentScene = std::move(battle);
            if (m_hWnd) SetWindowTextW(m_hWnd, L"TRPG - バトル");
            break;
        }
    case SceneType::RESULT:
        currentScene = std::make_unique<Result>();
        if (m_hWnd) SetWindowTextW(m_hWnd, L"TRPG - リザルト");
        break;
    }

    if (currentScene) {
        if (m_devMode) ::Log::Log(::Log::Level::Debug, "SceneManager: initializing new scene...");
        try {
            currentScene->Initialize();
            if (m_devMode) ::Log::Log(::Log::Level::Debug, "SceneManager: scene initialized successfully");
        }
        catch (const std::exception& ex) {
            std::string s = std::string("SceneManager: exception during scene Initialize: ") + ex.what();
            ::Log::Log(::Log::Level::Error, s);
        }
        catch (...) {
            ::Log::Log(::Log::Level::Error, "SceneManager: unknown exception during scene Initialize");
        }
    }
    else ::Log::Log(::Log::Level::Warning, "SceneManager::ChangeScene resulted in currentScene == nullptr");
}

SceneType SceneManager::GetCurrentScene() const {
    return currentType;
}

void SceneManager::Finalize() {}