#include "ScenarioLoader.h"
#include "system/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Scenario
{
    static ScenarioData ParseJsonToScenario(const json& j)
    {
        ScenarioData s;
        if (j.contains("id")) s.id = j.at("id").get<std::string>();
        if (j.contains("title")) s.title = j.at("title").get<std::string>();
        if (j.contains("description")) s.description = j.at("description").get<std::string>();
        if (j.contains("type")) s.type = j.at("type").get<std::string>();
        if (j.contains("recommendedPlayers")) s.recommendedPlayers = j.at("recommendedPlayers").get<int>();
        if (j.contains("progress")) s.progress = j.at("progress").get<int>();
        if (j.contains("warning")) s.warning = j.at("warning").get<std::string>();

        if (j.contains("enemies") && j.at("enemies").is_array()) {
            for (auto& je : j.at("enemies")) {
                Enemy e;
                if (je.contains("name")) e.name = je.at("name").get<std::string>();
                if (je.contains("hp")) e.hp = je.at("hp").get<int>();
                if (je.contains("atk")) e.atk = je.at("atk").get<int>();
                if (je.contains("fearDamage")) e.fearDamage = je.at("fearDamage").get<int>();
                s.enemies.push_back(e);
            }
        }

        if (j.contains("events") && j.at("events").is_array()) {
            for (auto& je : j.at("events")) {
                Event ev;
                if (je.contains("trigger")) ev.trigger = je.at("trigger").get<std::string>();
                if (je.contains("sanityLoss")) ev.sanityLoss = je.at("sanityLoss").get<int>();
                if (je.contains("text")) ev.text = je.at("text").get<std::string>();
                s.events.push_back(ev);
            }
        }

        return s;
    }

    std::optional<ScenarioData> LoadScenarioFromFile(const std::string& filepath)
    {
        try {
            std::ifstream ifs(filepath);
            if (!ifs.is_open()) {
                std::cerr << "ScenarioLoader: Failed to open " << filepath << "\n";
                return std::nullopt;
            }
            json j;
            ifs >> j;
            // 基本的なバリデーション
            if (!j.contains("id") || !j.contains("title")) {
                std::cerr << "ScenarioLoader: missing required fields in " << filepath << "\n";
                return std::nullopt;
            }
            ScenarioData s = ParseJsonToScenario(j);
            return s;
        } catch (const json::parse_error& e) {
            std::cerr << "ScenarioLoader: parse error in " << filepath << " : " << e.what() << "\n";
            return std::nullopt;
        } catch (const std::exception& e) {
            std::cerr << "ScenarioLoader: exception in " << filepath << " : " << e.what() << "\n";
            return std::nullopt;
        }
    }

    std::vector<ScenarioData> LoadScenariosFromDirectory(const std::string& directoryPath)
    {
        std::vector<ScenarioData> out;
        try {
            namespace fs = std::filesystem;
            for (auto& p : fs::directory_iterator(directoryPath)) {
                if (!p.is_regular_file()) continue;
                auto ext = p.path().extension().string();
                if (ext == ".json" || ext == ".JSON") {
                    auto opt = LoadScenarioFromFile(p.path().string());
                    if (opt) out.push_back(std::move(*opt));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "ScenarioLoader: directory iteration failed: " << e.what() << "\n";
        }
        return out;
    }
}