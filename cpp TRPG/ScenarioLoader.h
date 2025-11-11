#pragma once
#include <string>
#include <vector>
#include <optional>

namespace Scenario
{
    struct Enemy {
        std::string name;
        int hp = 0;
        int atk = 0;
        int fearDamage = 0;
    };

    struct Event {
        std::string trigger;
        int sanityLoss = 0;
        std::string text;
    };

    struct ScenarioData {
        std::string id;
        std::string title;
        std::string description;
        std::string type; // "single" / "group"
        int recommendedPlayers = 1;
        int progress = 0;
        std::string warning;
        std::vector<Enemy> enemies;
        std::vector<Event> events;
    };

    // ファイルから 1 つのシナリオをロード（失敗時は nullopt）
    std::optional<ScenarioData> LoadScenarioFromFile(const std::string& filepath);

    // ディレクトリ内の *.json を全走査してロード（成功したもののみ返す）
    std::vector<ScenarioData> LoadScenariosFromDirectory(const std::string& directoryPath);
}