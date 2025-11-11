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
    ImGui::Text("Spase:Title");



    ImGui::End();
}

void Result::Render() {
    // タイトルの描画（文字や背景など）
}