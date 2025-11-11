#include "TitleScene.h"
#include "SceneManager.h"

// DirectX関連の描画を記述
//タイトル・（録画の確認）あったらいいな



void TitleScene::Initialize() {


    // 画像/音声ロードなど
}

void TitleScene::Update() {
    ImGui::Begin("TitleScene");

    ImGui::Text("NowScene:TITLE");
    ImGui::Text("SpaceKeyMove");

    if (ImGui::Button("TRPGSLECT")) {
        g_SceneManager.ChangeScene(SceneType::TRPG_SELECT);
    }

    ImGui::End();
}


void TitleScene::Render() {
    // タイトルの描画（文字や背景など）
}