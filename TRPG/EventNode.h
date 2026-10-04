#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include "system/json.hpp"

using json = nlohmann::json;

// このヘッダは探索ノードと選択肢（Choice）を表現します。
// JSON からノードを読み込み、選択肢の要件や効果をデータ駆動で表現するための構造体群を定義します。

// サイコロ判定の条件を表します（面数、閾値、比較方法）
// サイコロ判定の条件を表します。
// - sides: サイコロの面数（例:6）
// - threshold: 判定で比較する閾値
// - greaterOrEqual: true の場合は roll >= threshold で成功判定
struct RollCond {
    int sides = 6;
    int threshold = 0; // success if roll >= threshold when greaterOrEqual true
    bool greaterOrEqual = true;
};

// 1つの選択肢（画面上に表示される選択肢ボタン）を表現します。
// 要件(require_*)、既存のレガシーフィールド、そしてデータ駆動化された effects を保持します。
// Choice: シナリオ内の選択肢を表す構造体
// - text: 画面に表示される説明文
// - nextNodeID: 選択時に遷移するノード ID（通常の分岐）
// - rollCond: ロール判定がある場合は RollCond を設定する
// - requireFlags / requireInventory 等: 選択肢自体が有効となるための要件
// - setFlags / addInventory 等の既存フィールドは互換性のため残すが、
//   内部では data-driven な Effect に変換して実行されます。
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
    // inventory requirements / effects
    std::vector<std::string> requireInventory;
    std::vector<std::string> addInventory;
    std::vector<std::string> removeInventory;
    // numeric variable requirements / effects
    std::unordered_map<std::string,int> requireVarMin;
    std::unordered_map<std::string,int> requireVarMax;
    std::unordered_map<std::string,int> addVar;
    std::unordered_map<std::string,int> setVar;
    // データ駆動化された効果（Effect）
    // JSON の "effects" 配列から読み込まれ、実行時に評価されます。
    struct Effect {
        std::string op; // e.g. "add_inventory", "remove_inventory", "add_var", "set_var", "set_flag", "clear_flag"
        std::string key; // for var name or single-item ops
        int intValue = 0; // numeric value for var ops
        std::vector<std::string> items; // list of items for inventory/flags
        // 効果実行の条件（この条件が満たされると効果が適用される）
        struct Condition {
            enum class Mode { ALL=0, ANY=1 } mode = Mode::ALL;
            std::vector<std::string> requireFlags;
            std::vector<std::string> requireNotFlags;
            std::vector<std::string> requireInventory;
            std::unordered_map<std::string,int> requireVarMin;
            std::unordered_map<std::string,int> requireVarMax;
        } condition;
        // 条件分岐サポート: if の then / else に該当する効果リスト
        std::vector<Effect> thenEffects; // 条件が true の場合に実行される効果群
        std::vector<Effect> elseEffects; // 条件が false の場合に実行される効果群
    };
    std::vector<Effect> effects; // base effects when choice taken
    std::vector<Effect> effectsOnSuccess; // extra effects applied on roll success
    std::vector<Effect> effectsOnFail; // extra effects applied on roll fail
};

// EventNode: 1つの探索ノード（テキストと複数の選択肢）を表します
struct EventNode {
    int id = -1;
    std::string text;
    // optional speaker id for multi-character support
    std::string speaker; // character id to mark as speaking
    std::string speakerExpression; // optional expression/variant for speaker
    std::vector<Choice> choices;
    // JSON から EventNode を構築するヘルパー関数
    // 既存のレガシーフィールドも effects に変換して互換性を保ちつつ、
    // 新しい data-driven な effects 配列をパースします。
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

                auto getIntMap = [&](const json &obj, const std::string &key, std::unordered_map<std::string,int> &out) {
                    if (!obj.contains(key)) return;
                    if (!obj[key].is_object()) return;
                    for (auto it = obj[key].begin(); it != obj[key].end(); ++it) {
                        if (it.value().is_number_integer()) out[it.key()] = it.value().get<int>();
                    }
                };

                if (cj.contains("text")) c.text = cj["text"].get<std::string>();
                if (cj.contains("next")) c.nextNodeID = cj["next"].get<int>();
                // speaker / speaker expression handled at node-level if present
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
                    // convert roll flag effects into commandized effects
                    for (auto &sf : c.setOnSuccess) { Choice::Effect e; e.op = "set_flag"; e.items.push_back(sf); c.effectsOnSuccess.push_back(e); }
                    for (auto &cf : c.clearOnSuccess) { Choice::Effect e; e.op = "clear_flag"; e.items.push_back(cf); c.effectsOnSuccess.push_back(e); }
                    for (auto &sf : c.setOnFail) { Choice::Effect e; e.op = "set_flag"; e.items.push_back(sf); c.effectsOnFail.push_back(e); }
                    for (auto &cf : c.clearOnFail) { Choice::Effect e; e.op = "clear_flag"; e.items.push_back(cf); c.effectsOnFail.push_back(e); }
                }
                // generic flag requirements / effects
                getStringArray(cj, "require_flags", c.requireFlags);
                getStringArray(cj, "require_not_flags", c.requireNotFlags);
                getStringArray(cj, "set_flags", c.setFlags);
                getStringArray(cj, "clear_flags", c.clearFlags);
                // inventory requirements / effects
                getStringArray(cj, "require_inventory", c.requireInventory);
                getStringArray(cj, "add_inventory", c.addInventory);
                getStringArray(cj, "remove_inventory", c.removeInventory);
                // numeric variable requirements / effects
                getIntMap(cj, "require_var_min", c.requireVarMin);
                getIntMap(cj, "require_var_max", c.requireVarMax);
                getIntMap(cj, "add_var", c.addVar);
                getIntMap(cj, "set_var", c.setVar);
                // build commandized effects from legacy fields (backwards compatibility)
                for (auto &it : c.addInventory) {
                    Choice::Effect e; e.op = "add_inventory"; e.items.push_back(it); c.effects.push_back(e);
                }
                for (auto &it : c.removeInventory) {
                    Choice::Effect e; e.op = "remove_inventory"; e.items.push_back(it); c.effects.push_back(e);
                }
                for (auto &kv : c.addVar) {
                    Choice::Effect e; e.op = "add_var"; e.key = kv.first; e.intValue = kv.second; c.effects.push_back(e);
                }
                for (auto &kv : c.setVar) {
                    Choice::Effect e; e.op = "set_var"; e.key = kv.first; e.intValue = kv.second; c.effects.push_back(e);
                }
                for (auto &sf : c.setFlags) {
                    Choice::Effect e; e.op = "set_flag"; e.items.push_back(sf); c.effects.push_back(e);
                }
                for (auto &cf : c.clearFlags) {
                    Choice::Effect e; e.op = "clear_flag"; e.items.push_back(cf); c.effects.push_back(e);
                }
                // roll-specific legacy effects mapping (if present)
                // parse add/remove var/inventory success/fail variants if provided explicitly
                // (keys like add_inventory_success / add_var_fail are handled below in 'effects' array parsing if used)

                // data-driven effects array parsing (new format) - each entry is an object with one op key
                if (cj.contains("effects") && cj["effects"].is_array()) {
                    // helper to parse a single op entry into effects vector
                    auto parseOpEntry = [&](const json &opobj, const Choice::Effect::Condition &cond, std::vector<Choice::Effect> &out) {
                        if (!opobj.is_object()) return;
                        for (auto it = opobj.begin(); it != opobj.end(); ++it) {
                            std::string op = it.key();
                            auto &val = it.value();
                            Choice::Effect e; e.op = op; e.condition = cond;
                            if (val.is_string()) {
                                e.items.push_back(val.get<std::string>());
                                out.push_back(e);
                            } else if (val.is_array()) {
                                for (auto &x : val) if (x.is_string()) e.items.push_back(x.get<std::string>());
                                out.push_back(e);
                            } else if (val.is_object()) {
                                for (auto itv = val.begin(); itv != val.end(); ++itv) {
                                    if (itv.value().is_number_integer()) {
                                        Choice::Effect ev; ev.op = op; ev.key = itv.key(); ev.intValue = itv.value().get<int>(); ev.condition = cond; out.push_back(ev);
                                    }
                                }
                            } else if (val.is_number_integer()) {
                                e.intValue = val.get<int>();
                                out.push_back(e);
                            }
                        }
                    };

                    for (auto &ef : cj["effects"]) {
                        if (!ef.is_object()) continue;
                        // optional condition object shared for ops in this entry
                        Choice::Effect::Condition cond;
                        if (ef.contains("require") && ef["require"].is_object()) {
                            auto &r = ef["require"];
                            getStringArray(r, "require_flags", cond.requireFlags);
                            getStringArray(r, "require_not_flags", cond.requireNotFlags);
                            getStringArray(r, "require_inventory", cond.requireInventory);
                            getIntMap(r, "require_var_min", cond.requireVarMin);
                            getIntMap(r, "require_var_max", cond.requireVarMax);
                            // optional logic mode: "all" (default) or "any"
                            if (r.contains("mode") && r["mode"].is_string()) {
                                std::string m = r["mode"].get<std::string>();
                                for (auto &c : m) c = (char)std::tolower((unsigned char)c);
                                if (m == "any" || m == "or") cond.mode = Choice::Effect::Condition::Mode::ANY;
                                else cond.mode = Choice::Effect::Condition::Mode::ALL;
                            }
                        }
                        // If this entry contains 'then' or 'else', treat it as an if-branch
                        if (ef.contains("then") || ef.contains("else")) {
                            Choice::Effect ife; ife.op = "if"; ife.condition = cond;
                            // parse then
                            if (ef.contains("then")) {
                                auto &tv = ef["then"];
                                if (tv.is_array()) {
                                    for (auto &te : tv) parseOpEntry(te, cond, ife.thenEffects);
                                } else {
                                    parseOpEntry(tv, cond, ife.thenEffects);
                                }
                            }
                            // parse else
                            if (ef.contains("else")) {
                                auto &evv = ef["else"];
                                if (evv.is_array()) {
                                    for (auto &te : evv) parseOpEntry(te, cond, ife.elseEffects);
                                } else {
                                    parseOpEntry(evv, cond, ife.elseEffects);
                                }
                            }
                            c.effects.push_back(ife);
                        } else {
                            // regular op entries
                            parseOpEntry(ef, cond, c.effects);
                        }
                    }
                }
                n.choices.push_back(c);
            }
        }
        return n;
    }
};
