#pragma once
#include <vector>
#include <string>
#include <fstream>
#include "system/json.hpp"

struct CharacterData {
    std::string name;
    std::string job;
    int str, con, dex, int_, pow, app, siz, edu;
    int san, ruk, ide, kow, dur;
};

class CharacterDataManager {
public:
    static CharacterDataManager& Instance() {
        static CharacterDataManager instance;
        return instance;
    }

    void AddCharacter(const CharacterData& data) {
        characters.push_back(data);
    }

    const std::vector<CharacterData>& GetCharacters() const {
        return characters;
    }

    // ファイル保存
    void Save(const std::string& failname) {
        nlohmann::json j;
        for (const auto& c : characters) {
            j.push_back({
                {"name", c.name},
                {"job", c.job},
                {"str", c.str},
                {"con", c.con},
                {"dex", c.dex},
                {"int_", c.int_},
                {"pow", c.pow},
                {"app", c.app},
                {"siz", c.siz},
                {"edu", c.edu},
                {"san", c.san},
                {"ruk", c.ruk},
                {"ide", c.ide},
                {"kow", c.kow},
                {"dur", c.dur}
                });
        }
        std::ofstream file(failname);
        file << j.dump(4); // 4スペースインデントで書き込み
    }

    // ファイル読み込み
    void Load(const std::string& failname) {
        characters.clear();
        std::ifstream file(failname);
        if (!file.is_open()) return;

        nlohmann::json j;
        file >> j;

        for (auto& elem : j) {
            CharacterData c;
            c.name = elem["name"];
            c.job = elem["job"];
            c.str = elem["str"];
            c.con = elem["con"];
            c.dex = elem["dex"];
            c.int_ = elem["int_"];
            c.pow = elem["pow"];
            c.app = elem["app"];
            c.siz = elem["siz"];
            c.edu = elem["edu"];
            c.san = elem["san"];
            c.ruk = elem["ruk"];
            c.ide = elem["ide"];
            c.kow = elem["kow"];
            c.dur = elem["dur"];
            characters.push_back(c);
        }
    }

private:
    CharacterDataManager() = default;
    std::vector<CharacterData> characters;
};
