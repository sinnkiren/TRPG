#include "ExploreScene.h"
#include "EventNode.h"
#include "CharacterSelect.h"
#include "system/json.hpp"
#include "AssetManager.h"
#include "system/imgui/imgui.h"
#include "Dice.h"
#include "DiceVisual.h"
#include "Logging.h"
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <Windows.h>
#include "SceneManager.h"

using json = nlohmann::json;

// ExploreScene: 探索用のノード読み込み、選択肢表示、状態管理（flags/inventory/vars）の実装ファイル
// このファイルは JSON ファイルから EventNode を読み込み、選択肢をレンダリングして
// ユーザ操作により GameState を更新・保存します。

static std::unordered_map<int, EventNode> g_nodes;
static std::vector<std::string> g_log;
// 中央のゲーム状態（探索シーンと共有される状態）
struct GameState {
    int currentNode = -1; // 現在のノードID
    std::unordered_set<std::string> flags; // 論理的なフラグ集合
    std::unordered_set<std::string> inventory; // 所持品（重複不可）
    std::unordered_map<std::string,int> vars; // 数値変数（例: san, gold 等）
};
static GameState g_state;
static std::string g_saveError; // セーブ/ロードエラーの説明

// セーブファイルのパスを決定して返す
/*
 * GetExploreSavePath
 * ------------------
 * セーブファイルのフルパスを返します。
 * - AssetManager を基準に保存先を決定します。
 * - テストや開発時は実行フォルダに "explore_save.json" を作成します。
 */
static std::string GetExploreSavePath()
{
    // Save next to asset root for simplicity
    std::string root = AssetManager::GetAssetRoot();
    if (root.empty()) return std::string("explore_save.json");
    // ensure no trailing slash issues
    char last = root.back();
    if (last == '/' || last == '\\') return root + "explore_save.json";
    return root + "/explore_save.json";
}
// forward declarations of static helpers (defined later)
static void SaveExploreState();
static void LoadExploreState();

// Save format version. Increment when changing saved JSON layout.
static constexpr int kExploreSaveVersion = 1;

// 現在の g_state を JSON としてディスクに書き出す
// players は SceneManager のロスターから取得して保存する
/*
 * SaveExploreState
 * ----------------
 * 現在の探索状態（g_state）と SceneManager に登録されたプレイヤーロスターを
 * JSON にシリアライズしてディスクに保存します。
 * - ここで保存される内容: saveVersion, currentNode, flags, inventory, vars, players, activePlayerIndex
 * - 失敗時は g_saveError にメッセージを格納しログ出力します。
 */
static void SaveExploreState()
{
    std::string path = GetExploreSavePath();
    json j;
    j["saveVersion"] = kExploreSaveVersion;
    j["currentNode"] = g_state.currentNode;
    j["flags"] = json::array();
    for (auto &f : g_state.flags) j["flags"].push_back(f);
    j["inventory"] = json::array();
    for (auto &it : g_state.inventory) j["inventory"].push_back(it);
    j["vars"] = json::object();
    for (auto &kv : g_state.vars) j["vars"][kv.first] = kv.second;
    // players: serialize SceneManager roster
    j["players"] = json::array();
    {
        const auto &players = g_SceneManager.GetPlayers();
        for (const auto &p : players) {
            json pj;
            pj["name"] = p.name;
            pj["job"] = p.job;
            pj["str"] = p.str; pj["con"] = p.con; pj["dex"] = p.dex; pj["int"] = p.int_;
            pj["pow"] = p.pow; pj["cha"] = p.cha; pj["app"] = p.app; pj["siz"] = p.siz; pj["edu"] = p.edu;
            pj["sanity"] = p.sanity; pj["maxSanity"] = p.maxSanity;
            pj["endurance"] = p.endurance; pj["maxEndurance"] = p.maxEndurance;
            pj["portraitPath"] = p.portraitPath;
            pj["skills"] = json::array();
            for (auto &s : p.skills) pj["skills"].push_back(s);
            j["players"].push_back(pj);
        }
        j["activePlayerIndex"] = g_SceneManager.GetActivePlayerIndex();
    }
    // inventory 保存処理
    try {
        std::ofstream ofs(path);
        if (!ofs.is_open()) { g_saveError = "Failed to open save file for writing: " + path; ::Log::Log(::Log::Level::Error, g_saveError); return; }
        ofs << j.dump(2);
        g_saveError.clear();
        ::Log::Log(::Log::Level::Info, std::string("Explore: saved state to: ") + path);
    } catch (const std::exception &ex) {
        g_saveError = std::string("Save failed: ") + ex.what();
        ::Log::Log(::Log::Level::Error, g_saveError);
    }
}

// ディスクから JSON を読み込み g_state と SceneManager のプレイヤーロスターを復元する
/*
 * LoadExploreState
 * ----------------
 * セーブファイルを読み込み、g_state と SceneManager のプレイヤーロスターを復元します。
 * - 互換性のため saveVersion を確認し、既知フィールドのみ復元します。
 * - 不正な JSON やファイルが無い場合は g_saveError を更新します。
 */
static void LoadExploreState()
{
    std::string path = GetExploreSavePath();
    namespace fs = std::filesystem;
    if (!fs::exists(path)) { g_saveError = ""; return; }
    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) { g_saveError = "Failed to open save file: " + path; ::Log::Log(::Log::Level::Error, g_saveError); return; }
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        if (!json::accept(content)) { g_saveError = "Save file JSON invalid: " + path; ::Log::Log(::Log::Level::Error, g_saveError); return; }
        json j = json::parse(content);
        int ver = j.value("saveVersion", 0);
        if (ver == 0) {
            ::Log::Log(::Log::Level::Info, std::string("Explore: loading legacy save (no version) from: ") + path);
        } else if (ver != kExploreSaveVersion) {
            if (ver > kExploreSaveVersion) ::Log::Log(::Log::Level::Warning, std::string("Explore: save file version is newer (file=") + std::to_string(ver) + ", supported=" + std::to_string(kExploreSaveVersion) + ")");
            else ::Log::Log(::Log::Level::Info, std::string("Explore: save file version different (file=") + std::to_string(ver) + ", supported=" + std::to_string(kExploreSaveVersion) + ")");
        }
        if (j.contains("currentNode")) {
            int loaded = j["currentNode"].get<int>();
            // validate against loaded nodes; if invalid, keep current default
            if (g_nodes.count(loaded)) g_state.currentNode = loaded;
            else {
                // fallback to node 0 if exists, otherwise first node
                if (g_nodes.count(0)) g_state.currentNode = 0;
                else if (!g_nodes.empty()) g_state.currentNode = g_nodes.begin()->first;
            }
        }
        if (j.contains("flags") && j["flags"].is_array()) {
            g_state.flags.clear();
            for (auto &e : j["flags"]) if (e.is_string()) g_state.flags.insert(e.get<std::string>());
        }
        if (j.contains("inventory") && j["inventory"].is_array()) {
            g_state.inventory.clear();
            for (auto &e : j["inventory"]) if (e.is_string()) g_state.inventory.insert(e.get<std::string>());
        }
        if (j.contains("vars") && j["vars"].is_object()) {
            g_state.vars.clear();
            for (auto it = j["vars"].begin(); it != j["vars"].end(); ++it) {
                if (it.value().is_number_integer()) g_state.vars[it.key()] = it.value().get<int>();
            }
        }
        // load players if present
        if (j.contains("players") && j["players"].is_array()) {
            auto &plist = g_SceneManager.GetPlayers();
            plist.clear();
            for (auto &pj : j["players"]) {
                CharcterScene::CharcterDate p;
                if (pj.contains("name")) p.name = pj["name"].get<std::string>();
                if (pj.contains("job")) p.job = pj["job"].get<std::string>();
                if (pj.contains("str")) p.str = pj["str"].get<int>();
                if (pj.contains("con")) p.con = pj["con"].get<int>();
                if (pj.contains("dex")) p.dex = pj["dex"].get<int>();
                if (pj.contains("int")) p.int_ = pj["int"].get<int>();
                if (pj.contains("pow")) p.pow = pj["pow"].get<int>();
                if (pj.contains("cha")) p.cha = pj["cha"].get<int>();
                if (pj.contains("app")) p.app = pj["app"].get<int>();
                if (pj.contains("siz")) p.siz = pj["siz"].get<int>();
                if (pj.contains("edu")) p.edu = pj["edu"].get<int>();
                if (pj.contains("sanity")) p.sanity = pj["sanity"].get<int>();
                if (pj.contains("maxSanity")) p.maxSanity = pj["maxSanity"].get<int>();
                if (pj.contains("endurance")) p.endurance = pj["endurance"].get<int>();
                if (pj.contains("maxEndurance")) p.maxEndurance = pj["maxEndurance"].get<int>();
                if (pj.contains("portraitPath")) p.portraitPath = pj["portraitPath"].get<std::string>();
                if (pj.contains("skills") && pj["skills"].is_array()) {
                    p.skills.clear();
                    for (auto &s : pj["skills"]) if (s.is_string()) p.skills.push_back(s.get<std::string>());
                }
                plist.push_back(p);
            }
            if (j.contains("activePlayerIndex") && j["activePlayerIndex"].is_number_integer()) {
                int idx = j["activePlayerIndex"].get<int>();
                g_SceneManager.SetActivePlayerIndex(idx);
            }
        }
        g_saveError.clear();
        ::Log::Log(::Log::Level::Info, std::string("Explore: loaded save from: ") + path);
    } catch (const std::exception &ex) {
        g_saveError = std::string("Load failed: ") + ex.what();
        ::Log::Log(::Log::Level::Error, g_saveError);
    }
}
// UI 表示のためにノード遷移を少し遅らせるための状態
static bool g_waitingChoice = false;
static int g_pendingNode = -1;
static float g_pendingTimer = 0.0f;

/*
 * ExploreScene::Initialize
 * -------------------------
 * - アセットから explore.json を読み込んで EventNode を構築します。
 * - g_nodes にノードを格納し、開始ノードを決定します。
 * - セーブファイルがあれば LoadExploreState() を呼んで状態を復元します。
 */
void ExploreScene::Initialize() {
    // load nodes from assets/story/explore.json if present
    namespace fs = std::filesystem;
    // Use AssetManager to obtain story path
    std::string path = AssetManager::GetStoryPath("explore.json");
    g_nodes.clear();
    g_log.clear();
    g_state.currentNode = -1;
    g_state.flags.clear();
    g_state.inventory.clear();
    g_state.vars.clear();
    g_waitingChoice = false;

    if (!fs::exists(path)) {
        g_log.push_back(std::string("Explore: file not found: ") + path);
        ::Log::Log(::Log::Level::Warning, std::string("ExploreScene: explore.json not found: ") + path);
        return;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        g_log.push_back(std::string("Explore: failed to open file: ") + path);
        ::Log::Log(::Log::Level::Warning, std::string("ExploreScene: failed to open: ") + path);
        return;
    }

    // Read file into string so we can validate before parsing (avoids first-chance exceptions in debugger)
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    json j;
    try {
        if (!json::accept(content)) {
            g_log.push_back(std::string("Explore: JSON invalid (accept failed): ") + path);
            ::Log::Log(::Log::Level::Error, std::string("ExploreScene: JSON invalid (accept failed): ") + path);
            return;
        }
        j = json::parse(content);
    } catch (const std::exception &ex) {
        g_log.push_back(std::string("Explore: JSON parse error: ") + ex.what());
        ::Log::Log(::Log::Level::Error, std::string("ExploreScene: JSON parse error: ") + ex.what());
        return;
    }
    if (!j.is_array()) return;
    for (auto &it : j) {
        EventNode n = EventNode::FromJson(it);
        if (n.id >= 0) g_nodes[n.id] = n;
    }
    // start at node 0 if exists, otherwise pick first node available
    if (g_nodes.empty()) {
        g_log.push_back("Explore: no nodes loaded from JSON.");
        ::Log::Log(::Log::Level::Warning, "ExploreScene: no nodes loaded from explore.json");
        g_state.currentNode = -1;
    } else {
        if (g_nodes.count(0)) g_state.currentNode = 0;
        else {
            // pick first available node id
            g_state.currentNode = g_nodes.begin()->first;
            g_log.push_back(std::string("Explore: start node 0 not found, using node ") + std::to_string(g_state.currentNode));
        }
        g_log.push_back(std::string("Explore: loaded nodes: ") + std::to_string((int)g_nodes.size()));
        ::Log::Log(::Log::Level::Info, std::string("ExploreScene: loaded nodes from: ") + path + ", count=" + std::to_string((int)g_nodes.size()));
    }
    // try to load saved exploration state (if any)
    LoadExploreState();
}

/*
 * ExploreScene::Update
 * ---------------------
 * 毎フレームの更新処理。
 * - DiceVisual を更新してダイス演出を進めます。
 * - 選択肢の後処理として遷移を遅延させるための pending タイマーを扱います。
 */
void ExploreScene::Update() {
    // advance dice visual
    if (ImGui::GetCurrentContext()) {
        ImGuiIO &io = ImGui::GetIO();
        float dt = io.DeltaTime;
        if (dt <= 0.0f) dt = 1.0f/60.0f;
        DiceVisual::Instance().Update(dt);
        // handle pending transition timer
        if (g_waitingChoice) {
            g_pendingTimer -= dt;
            if (g_pendingTimer <= 0.0f) {
                if (g_pendingNode >= 0 && g_nodes.count(g_pendingNode)) {
                    g_state.currentNode = g_pendingNode;
                    // save state after transition
                    SaveExploreState();
                }
                g_waitingChoice = false;
                g_pendingNode = -1;
                g_pendingTimer = 0.0f;
            }
        }
    }
}

/*
 * ExploreScene::RenderUI
 * -----------------------
 * 開発用のデバッグ UI を描画します。
 * - flags / inventory / vars の中身を表示・編集できます。
 * - Save / Load ボタンで手動保存や復元が可能です。
 */
void ExploreScene::RenderUI() {
    if (ImGui::GetCurrentContext() == nullptr) return;
    ImGui::Begin("Explore (Dev)");
    ImGui::Text("Current node: %d", g_state.currentNode);
    // show flags
    ImGui::Separator();
    ImGui::Text("Flags:");
    ImGui::BeginChild("Flags", ImVec2(0,80), true);
    for (auto &f : g_state.flags) ImGui::TextWrapped("%s", f.c_str());
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::Text("Inventory:");
    ImGui::BeginChild("Inventory", ImVec2(0,80), true);
    int idx = 0;
    for (auto it = g_state.inventory.begin(); it != g_state.inventory.end();) {
        ImGui::PushID(idx);
        ImGui::TextWrapped("%s", it->c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            it = g_state.inventory.erase(it);
            ImGui::PopID();
            continue;
        } else ++it;
        ImGui::PopID();
        ++idx;
    }
    ImGui::EndChild();
    static char newItemBuf[128] = "";
    ImGui::InputText("New Item", newItemBuf, sizeof(newItemBuf));
    ImGui::SameLine(); if (ImGui::Button("Add Item") && newItemBuf[0] != '\0') { g_state.inventory.insert(std::string(newItemBuf)); newItemBuf[0] = '\0'; }
    if (ImGui::Button("Log Clear")) g_log.clear();
    ImGui::SameLine();
    if (ImGui::Button("Save State")) { SaveExploreState(); }
    ImGui::SameLine();
    if (ImGui::Button("Load State")) { LoadExploreState(); }
    if (!g_saveError.empty()) ImGui::TextWrapped("Save/Load error: %s", g_saveError.c_str());
    ImGui::Separator();
    ImGui::BeginChild("Log", ImVec2(0,200), true);
    for (auto &s : g_log) ImGui::TextWrapped("%s", s.c_str());
    ImGui::EndChild();
    ImGui::End();
}

/*
 * ExploreScene::Render
 * ---------------------
 * 探索ダイアログ（ノードテキストと選択肢）を描画します。
 * - 各選択肢の要件チェックを行い、ボタン押下で効果を適用します。
 * - ロール判定がある選択肢はダイスを振り、成功/失敗で分岐させます。
 * - 効果の適用は ApplyChoiceEffects に委譲し、状態更新後に自動セーブします。
 */
void ExploreScene::Render() {
    if (g_state.currentNode < 0 || g_nodes.find(g_state.currentNode) == g_nodes.end()) {
        // show a helpful message so user knows why nothing is displayed
        if (ImGui::GetCurrentContext() == nullptr) return;
        ImGuiIO &io = ImGui::GetIO();
        ImVec2 vp = io.DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(vp.x*0.25f, vp.y*0.4f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(vp.x*0.5f, vp.y*0.2f), ImGuiCond_Always);
        ImGui::Begin("Explore (Info)", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        ImGui::TextWrapped("No exploration node active.");
        ImGui::Separator();
        ImGui::TextWrapped("Nodes loaded: %d", (int)g_nodes.size());
        if (!g_nodes.empty()) {
            ImGui::TextWrapped("First node id: %d", g_nodes.begin()->first);
        }
        ImGui::End();
        return;
    }
    const EventNode &n = g_nodes[g_state.currentNode];

    // center dialog near bottom by default but ensure visible on most resolutions
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 vp = io.DisplaySize;
    ImVec2 winPos = ImVec2(vp.x * 0.05f, vp.y * 0.58f);
    ImVec2 winSize = ImVec2(vp.x * 0.6f, vp.y * 0.28f);
    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);
    ImGui::Begin("Explore", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped("%s", n.text.c_str());
    ImGui::Separator();

    // choices
    // 選択肢の実行に関わるヘルパ
    // ApplyFlags: フラグの追加/削除をまとめて行う
    auto ApplyFlags = [&](const std::vector<std::string> &sets, const std::vector<std::string> &clears) {
        for (auto &f : sets) g_state.flags.insert(f);
        for (auto &f : clears) g_state.flags.erase(f);
    };

    // apply commandized effects for a given choice
    // Choice::effects のコマンドを評価・実行するコア処理。
    // success 引数はロール判定の結果（成功:true/失敗:false）を示す。
    auto ApplyChoiceEffects = [&](const Choice &c, bool success) {
        // 条件評価: Condition を見て現在の g_state が条件を満たすか判定する
        auto CheckCond = [&](const Choice::Effect::Condition &cond)->bool {
            // If no requirements specified, condition passes
            bool hasAnyReq = !cond.requireFlags.empty() || !cond.requireNotFlags.empty() || !cond.requireInventory.empty()
                || !cond.requireVarMin.empty() || !cond.requireVarMax.empty();
            if (!hasAnyReq) return true;

            if (cond.mode == Choice::Effect::Condition::Mode::ALL) {
                // flags
                for (auto &rf : cond.requireFlags) if (!g_state.flags.count(rf)) return false;
                for (auto &nf : cond.requireNotFlags) if (g_state.flags.count(nf)) return false;
                // inventory
                for (auto &it : cond.requireInventory) if (!g_state.inventory.count(it)) return false;
                // vars min/max
                for (auto &kv : cond.requireVarMin) {
                    int val = 0;
                    auto itv = g_state.vars.find(kv.first);
                    if (itv != g_state.vars.end()) val = itv->second;
                    if (val < kv.second) return false;
                }
                for (auto &kv : cond.requireVarMax) {
                    int val = 0;
                    auto itv = g_state.vars.find(kv.first);
                    if (itv != g_state.vars.end()) val = itv->second;
                    if (val > kv.second) return false;
                }
                return true;
            }
            else {
                // Mode::ANY - satisfy if any single requirement is met
                // flags: any listed flag present
                for (auto &rf : cond.requireFlags) if (g_state.flags.count(rf)) return true;
                // requireNotFlags: if none of the listed flags present, that's a satisfied condition
                if (!cond.requireNotFlags.empty()) {
                    bool anyPresent = false;
                    for (auto &nf : cond.requireNotFlags) if (g_state.flags.count(nf)) { anyPresent = true; break; }
                    if (!anyPresent) return true;
                }
                // inventory: any listed item present
                for (auto &it : cond.requireInventory) if (g_state.inventory.count(it)) return true;
                // vars min: any variable meets its min
                for (auto &kv : cond.requireVarMin) {
                    int val = 0;
                    auto itv = g_state.vars.find(kv.first);
                    if (itv != g_state.vars.end()) val = itv->second;
                    if (val >= kv.second) return true;
                }
                // vars max: any variable meets its max constraint
                for (auto &kv : cond.requireVarMax) {
                    int val = 0;
                    auto itv = g_state.vars.find(kv.first);
                    if (itv != g_state.vars.end()) val = itv->second;
                    if (val <= kv.second) return true;
                }
                return false;
            }
        };
        // 実行対象の効果リストを作成（基本効果 + success/fail 用効果）
        std::vector<Choice::Effect> exec = c.effects;
        if (success) {
            exec.insert(exec.end(), c.effectsOnSuccess.begin(), c.effectsOnSuccess.end());
        } else {
            exec.insert(exec.end(), c.effectsOnFail.begin(), c.effectsOnFail.end());
        }
        // 再帰実行子: if/then/else をサポートしつつ各 effect を評価・適用する
        std::function<void(const std::vector<Choice::Effect>&, bool)> ExecEffects;
        ExecEffects = [&](const std::vector<Choice::Effect> &effectsList, bool parentSuccess) {
            for (auto &e : effectsList) {
                if (e.op == "if") {
                    // if エントリ: 条件を評価して then/else を再帰実行
                    bool condOk = CheckCond(e.condition);
                    g_log.push_back(std::string("IfEffect: ") + (condOk ? "(cond OK)" : "(cond FAIL)"));
                    if (condOk) ExecEffects(e.thenEffects, parentSuccess);
                    else ExecEffects(e.elseEffects, parentSuccess);
                    continue;
                }
                // 通常の effect: 条件を評価して適用
                bool condOk = CheckCond(e.condition);
                std::string dbg = std::string("Effect: ") + e.op + (condOk ? " (cond OK)" : " (cond FAIL)");
                g_log.push_back(dbg);
                if (!condOk) continue;
                if (e.op == "add_inventory") {
                    for (auto &it : e.items) { g_state.inventory.insert(it); g_log.push_back(std::string("Added item: ") + it); }
                } else if (e.op == "remove_inventory") {
                    for (auto &it : e.items) { g_state.inventory.erase(it); g_log.push_back(std::string("Removed item: ") + it); }
                } else if (e.op == "add_var") {
                    g_state.vars[e.key] += e.intValue;
                    g_log.push_back(std::string("Var add: ") + e.key + "=" + std::to_string(e.intValue));
                } else if (e.op == "set_var") {
                    g_state.vars[e.key] = e.intValue;
                    g_log.push_back(std::string("Var set: ") + e.key + "=" + std::to_string(e.intValue));
                } else if (e.op == "set_flag") {
                    for (auto &f : e.items) g_state.flags.insert(f);
                } else if (e.op == "clear_flag") {
                    for (auto &f : e.items) g_state.flags.erase(f);
                } else {
                    // 未知の op はログに残して無視
                    g_log.push_back(std::string("Unknown effect op: ") + e.op);
                }
            }
        };

        ExecEffects(exec, success);
    };

    for (size_t i = 0; i < n.choices.size(); ++i) {
        const Choice &c = n.choices[i];
        ImGui::PushID((int)i);

        // check requirements
        bool enabled = true;
        for (auto &rf : c.requireFlags) if (!g_state.flags.count(rf)) { enabled = false; break; }
        if (enabled) {
            for (auto &nf : c.requireNotFlags) if (g_state.flags.count(nf)) { enabled = false; break; }
        }
        // inventory requirements
        if (enabled && !c.requireInventory.empty()) {
            for (auto &it : c.requireInventory) {
                if (!g_state.inventory.count(it)) { enabled = false; break; }
            }
        }
        // numeric var requirements (min/max)
        if (enabled && !c.requireVarMin.empty()) {
            for (auto &kv : c.requireVarMin) {
                int val = 0;
                auto itv = g_state.vars.find(kv.first);
                if (itv != g_state.vars.end()) val = itv->second;
                if (val < kv.second) { enabled = false; break; }
            }
        }
        if (enabled && !c.requireVarMax.empty()) {
            for (auto &kv : c.requireVarMax) {
                int val = 0;
                auto itv = g_state.vars.find(kv.first);
                if (itv != g_state.vars.end()) val = itv->second;
                if (val > kv.second) { enabled = false; break; }
            }
        }

        if (!c.rollCond.has_value()) {
            if (!enabled) {
                ImGui::BeginDisabled();
                ImGui::Button(c.text.c_str(), ImVec2(-1, 0));
                ImGui::EndDisabled();
            } else {
                if (ImGui::Button(c.text.c_str(), ImVec2(-1, 0))) {
                    g_log.push_back(std::string("Choice: ") + c.text);
                    // apply flags immediately
                    ApplyFlags(c.setFlags, c.clearFlags);
                            // apply inventory/var effects
                            ApplyChoiceEffects(c, false);
                    if (c.nextNodeID >= 0 && g_nodes.count(c.nextNodeID)) {
                        // small delay so player sees UI feedback
                        g_pendingNode = c.nextNodeID;
                        g_pendingTimer = 0.18f;
                        g_waitingChoice = true;
                        // autosave after taking a choice
                        SaveExploreState();
                    } else g_log.push_back("Choice leads nowhere.");
                }
            }
        } else {
            // roll button shows condition
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s (Roll %d)", c.text.c_str(), c.rollCond->sides);
            if (!enabled) {
                ImGui::BeginDisabled();
                ImGui::Button(buf, ImVec2(-1, 0));
                ImGui::EndDisabled();
            } else if (ImGui::Button(buf, ImVec2(-1, 0))) {
                // perform roll (no immediate node change)
                int r = Dice::RollDieNoVisual(c.rollCond->sides);
                // visual
                std::vector<int> faces = { r };
                DiceVisual::Instance().StartRollFaces(c.rollCond->sides, faces);
                bool success = c.rollCond->greaterOrEqual ? (r >= c.rollCond->threshold) : (r <= c.rollCond->threshold);
                std::string msg = std::string("Rolled: ") + std::to_string(r) + (success ? " (SUCCESS)" : " (FAIL)");
                g_log.push_back(msg);
                // set pending node depending on result, allow visual to play for 1.0s
                if (success) {
                    ApplyFlags(c.setOnSuccess, c.clearOnSuccess);
                    // apply inventory/var effects (success)
                    ApplyChoiceEffects(c, true);
                    g_pendingNode = (c.nextOnSuccess >= 0) ? c.nextOnSuccess : -1;
                } else {
                    ApplyFlags(c.setOnFail, c.clearOnFail);
                    // apply inventory/var effects (fail)
                    ApplyChoiceEffects(c, false);
                    g_pendingNode = (c.nextOnFail >= 0) ? c.nextOnFail : -1;
                }
                g_pendingTimer = 1.0f; // wait for dice to settle / display
                g_waitingChoice = true;
                // autosave after roll decision
                SaveExploreState();
            }
        }
        ImGui::PopID();
    }

    ImGui::NewLine();
    ImGui::Separator();
    if (ImGui::Button("Back to Title")) {
        g_SceneManager.ChangeScene(SceneType::TITLE);
    }
    ImGui::End();

    // draw dice visual overlay
    DiceVisual::Instance().Render();
}



// Public wrappers
void ExploreScene::SaveStateNow()
{
    SaveExploreState();
}

void ExploreScene::LoadStateNow()
{
    LoadExploreState();
}

