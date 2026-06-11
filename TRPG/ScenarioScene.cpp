#include "ScenarioScene.h"
#include "SceneManager.h"
#include "system/imgui/imgui.h"
#include "TextureManager.h"
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

    // Lazy-load the scenario image and display it inside the ImGui window.
    static ImTextureID s_scenarioTex = nullptr;
    static bool s_scenarioTexAttempted = false;
    static const char* s_scenarioTexPath = "texture/sina.png"; // place image under assets/texture/

    ImGui::Begin("Scenario Scene");
    ImGui::Text("Now:Scenario Scene");
    ImGui::Text("Proceed when ready");

    // Attempt texture load once (TextureManager must have been initialized by Application)
    if (!s_scenarioTex && !s_scenarioTexAttempted) {
        s_scenarioTexAttempted = true;
        s_scenarioTex = TextureManager::GetImGuiTextureID(s_scenarioTexPath);
        if (!s_scenarioTex) {
            ::Log::Log(::Log::Level::Warning, std::string("ScenarioScene: scenario image not found: ") + s_scenarioTexPath);
        }
    }

    // Display image: stretch to available content region (use all available width/height)
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float drawW = avail.x > 0.0f ? avail.x : 640.0f;
    // If the available height is provided by the layout, use it to stretch vertically as well.
    float drawH = avail.y > 0.0f ? avail.y : (drawW * 0.33f);
    ImVec2 imgSize(drawW, drawH);
    if (s_scenarioTex) {
        ImGui::Image(s_scenarioTex, imgSize);
    }
    else {
        // Reserve space and show placeholder text when image not available
        ImGui::Dummy(imgSize);
        ImGui::SameLine();
        ImGui::Text("(Scenario image not available)");
    }

    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Text("Available Scenarios (from ../assets/story):");
    // enumerate JSON files in assets/story
    namespace fs = std::filesystem;
    std::string assetDir = std::string("../assets/story");
    try {
        if (fs::exists(assetDir) && fs::is_directory(assetDir)) {
            for (auto &entry : fs::directory_iterator(assetDir)) {
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
                    std::string full = p.string();
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
            ImGui::Text("No scenario directory found: %s", assetDir.c_str());
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