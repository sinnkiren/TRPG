#include "Result.h"
#include "SceneManager.h"
// DirectX関連の描画を記述
//キャラクターの作成・選択・功績点のレベル上げ

void Result::Initialize() {
    // 画像/音声ロードなど
}

void Result::Update() {
    // 入力処理（Spaceでゲームスタート）
    ImGui::Begin("Result");
    ImGui::Text("Now:Result");

    // Show victory/defeat based on SceneManager flag
    bool victory = g_SceneManager.WasLastResultVictory();
    if (victory) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Victory!");
        ImGui::Text("You have defeated the enemy.");
    }
    else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Defeat...");
        ImGui::Text("Your HP reached zero.");
    }

    ImGui::Spacing();
    if (ImGui::Button("Return to Title")) {
        g_SceneManager.ChangeScene(SceneType::TITLE);
    }

    ImGui::End();
}

void Result::Render() {
    // タイトルの描画（文字や背景など）
}