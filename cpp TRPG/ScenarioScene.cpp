// --- ファイル冒頭に追加 ---
#include "ScenarioLoader.h"

// --- Initialize 内で ---
void ScenarioScene::Initialize()
{
    // シナリオが置かれているフォルダを指定（プロジェクト内の相対パス例）
    const std::string scenarioDir = "Data/Scenarios";
    scenarios = Scenario::LoadScenariosFromDirectory(scenarioDir);

    if (scenarios.empty()) {
        // UI 等でユーザーに通知する方法を用意してください
        // ここでは簡易ログ出力
        std::cout << "ScenarioScene: no scenarios found in " << scenarioDir << "\n";
    } else {
        // 最初のシナリオを選択する等の初期化
        selectedScenarioIndex = 0;
    }
}