#pragma once
#include <string>
#include <vector>
#include <optional>
#include "system/json.hpp"

using json = nlohmann::json;

struct RollCond {
    int sides = 6;
    int threshold = 0; // success if roll >= threshold when greaterOrEqual true
    bool greaterOrEqual = true;
};

struct Choice {
    std::string text;
    int nextNodeID = -1;
    // optional roll-based branching
    std::optional<RollCond> rollCond;
    int nextOnSuccess = -1;
    int nextOnFail = -1;
    // flags required to enable this choice
    std::vector<std::string> requireFlags;
    // flags that must NOT be present to enable this choice
    std::vector<std::string> requireNotFlags;
    // flags to set/clear when this choice is taken
    std::vector<std::string> setFlags;
    std::vector<std::string> clearFlags;
    // for roll branches: flags to set/clear on success/fail
    std::vector<std::string> setOnSuccess;
    std::vector<std::string> clearOnSuccess;
    std::vector<std::string> setOnFail;
    std::vector<std::string> clearOnFail;
};

struct EventNode {
    int id = -1;
    std::string text;
    std::vector<Choice> choices;
    // helper: load from json
    static EventNode FromJson(const json &j) {
        EventNode n;
        if (j.contains("id")) n.id = j["id"].get<int>();
        if (j.contains("text")) n.text = j["text"].get<std::string>();
        if (j.contains("choices") && j["choices"].is_array()) {
            for (auto &cj : j["choices"]) {
                Choice c;
                auto getStringArray = [&](const json &obj, const std::string &key, std::vector<std::string> &out) {
                    if (!obj.contains(key)) return;
                    if (obj[key].is_string()) {
                        out.push_back(obj[key].get<std::string>());
                    } else if (obj[key].is_array()) {
                        for (auto &e : obj[key]) if (e.is_string()) out.push_back(e.get<std::string>());
                    }
                };

                if (cj.contains("text")) c.text = cj["text"].get<std::string>();
                if (cj.contains("next")) c.nextNodeID = cj["next"].get<int>();
                if (cj.contains("roll")) {
                    auto r = cj["roll"];
                    RollCond rc;
                    if (r.contains("sides")) rc.sides = r["sides"].get<int>();
                    if (r.contains("threshold")) rc.threshold = r["threshold"].get<int>();
                    if (r.contains("greaterOrEqual")) rc.greaterOrEqual = r["greaterOrEqual"].get<bool>();
                    c.rollCond = rc;
                    if (cj.contains("next_success")) c.nextOnSuccess = cj["next_success"].get<int>();
                    if (cj.contains("next_fail")) c.nextOnFail = cj["next_fail"].get<int>();
                    // roll specific flag effects
                    getStringArray(cj, "set_success", c.setOnSuccess);
                    getStringArray(cj, "clear_success", c.clearOnSuccess);
                    getStringArray(cj, "set_fail", c.setOnFail);
                    getStringArray(cj, "clear_fail", c.clearOnFail);
                }
                // generic flag requirements / effects
                getStringArray(cj, "require_flags", c.requireFlags);
                getStringArray(cj, "require_not_flags", c.requireNotFlags);
                getStringArray(cj, "set_flags", c.setFlags);
                getStringArray(cj, "clear_flags", c.clearFlags);
                n.choices.push_back(c);
            }
        }
        return n;
    }
};
