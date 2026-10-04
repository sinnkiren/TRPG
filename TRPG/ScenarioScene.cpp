#include "ScenarioScene.h"
#include "SceneManager.h"
#include "system/imgui/imgui.h"
#include "AssetManager.h"
#include "Logging.h"
// DirectX関連の描画を記述
#include <filesystem>
#include <cctype>
//シナリオの選択（今考えているのは毒入りスープ）進行状況の記録の確認



void ScenarioScene::Initialize() {
    // 画像/音声ロードなど

}

void ScenarioScene::Update() {
    // 入力処理（Spaceでゲームスタート）

    ImGui::Begin("Scenario Scene");
    ImGui::Text("Now:Scenario Scene");
    ImGui::Text("Proceed when ready");

    // If SceneManager recorded a story load error, show it here so user understands what went wrong.
    const std::string &loadErr = g_SceneManager.GetLastStoryLoadError();
    if (!loadErr.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f,0.4f,0.4f,1.0f), "Scenario load error:");
        ImGui::TextWrapped("%s", loadErr.c_str());
        if (ImGui::Button("Dismiss")) {
            g_SceneManager.ClearLastStoryLoadError();
        }
        ImGui::Separator();
    }

    ImGui::Spacing();

    ImGui::Separator();
    const std::filesystem::path storyDir = std::filesystem::path(AssetManager::GetAssetRoot()) / "story";
    ImGui::Text("Available Scenarios (from %s):", storyDir.string().c_str());
    // enumerate JSON files in assets/story
    try {
        if (std::filesystem::exists(storyDir) && std::filesystem::is_directory(storyDir)) {
            for (auto &entry : std::filesystem::directory_iterator(storyDir)) {
                if (!entry.is_regular_file()) continue;
                auto p = entry.path();
                auto ext = p.extension().string();
                for (auto &c : ext) c = (char)std::tolower((unsigned char)c);
                if (ext != ".json") continue;
                std::string name = p.filename().string();
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::Button((std::string("Load & Play##") + name).c_str())) {
                    // request SceneManager to load this JSON when switching to StoryPlayer
                    // Preserve UTF-8 bytes when passing to loader
                    auto u8 = std::filesystem::absolute(p).u8string();
                    std::string full;
                    full.reserve(u8.size());
                    for (auto ch : u8) full.push_back(static_cast<char>(ch));
                    g_SceneManager.SetPendingStoryPath(full);
                    g_SceneManager.ChangeScene(SceneType::GAME_PLAY);
                }
                ImGui::SameLine();    
                if (ImGui::Button((std::string("Show Path##") + name).c_str())) {
                    ::Log::Log(::Log::Level::Info, std::string("Scenario path: ") + p.string());
                }
            }
        }
        else {
            ImGui::Text("No scenario directory found: %s", storyDir.string().c_str());
        }
    } catch (const std::exception& ex) {
        ::Log::Log(::Log::Level::Warning, std::string("ScenarioScene: failed to enumerate scenarios: ") + ex.what());
    }

    ImGui::Spacing();
    if (ImGui::Button("Go to Character Select")) {
        ::Log::Log(::Log::Level::Info, "ScenarioScene: Go button pressed -> ChangeScene(CHARACTER_SELECT)");
        g_SceneManager.ChangeScene(SceneType::CHARACTER_SELECT);
    }

    ImGui::End();
}

void ScenarioScene::Render() {
    // タイトルの描画（文字や背景など）
}