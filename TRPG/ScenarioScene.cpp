#include "ScenarioScene.h"
#include "SceneManager.h"
// DirectX関連の描画を記述
//シナリオの選択（今考えているのは毒入りスープ）進行状況の記録の確認



void ScenarioScene::Initialize() {
    // 画像/音声ロードなど

}

void ScenarioScene::Update() {
    // 入力処理（Spaceでゲームスタート）
    ImGui::Begin("Scenario Scene");
    ImGui::Text("Now:Scenario Scene");
    ImGui::Text("Spase:Charcter");



    ImGui::End();
}

void ScenarioScene::Render() {
    // タイトルの描画（文字や背景など）
}