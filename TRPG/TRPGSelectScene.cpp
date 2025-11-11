#include "TRPGSelectScene.h"
#include "SceneManager.h"
// DirectX関連の描画を記述
//TRPGの選択（今考えているのはクトゥルフのみ）


void TRPGSelectScene::Initialize() {
    // 画像/音声ロードなど
    
}

void TRPGSelectScene::Update() {
    ImGui::Begin("TRPG Select Scene");
    ImGui::Text("Now：TRPGScene");
    ImGui::Text("Spase:Scenario");


    ImGui::End();
}


void TRPGSelectScene::Render() {
    // タイトルの描画（文字や背景など）
}