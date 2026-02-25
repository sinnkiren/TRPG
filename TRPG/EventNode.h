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
                }
                n.choices.push_back(c);
            }
        }
        return n;
    }
};
