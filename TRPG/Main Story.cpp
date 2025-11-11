#include "Main Story.h"
#include "BattleScene.h"
#include "SceneManager.h"
#include "system/imgui/imgui.h"

void MainStory::Initialize() {
 // 戦闘シーンを作成
 battleScene = std::make_unique<BattleScene>();

 // SceneManager に保持されたプレイヤーデータがあれば渡す
 // （キャラクター選択画面で SetPlayer が呼ばれている想定）
 if (battleScene) {
 // SceneManagerからプレイヤーデータを取得して BattleScene に設定
 battleScene->SetPlayer(g_SceneManager.GetPlayer());
 battleScene->Initialize();
 }
}

void MainStory::Update() {
 // BattleScene に更新処理を委譲する
 if (battleScene) battleScene->Update();
}

void MainStory::Render() {
 // BattleScene に描画処理を委譲する
 if (battleScene) battleScene->Render();
}