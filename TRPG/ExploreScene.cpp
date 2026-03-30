#include "ExploreScene.h"
#include "EventNode.h"
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

static std::unordered_map<int, EventNode> g_nodes;
static std::vector<std::string> g_log;
// Centralized game state for exploration and future sharing across scenes
struct GameState {
    int currentNode = -1;
    std::unordered_set<std::string> flags;
    std::unordered_set<std::string> inventory; // use set for O(1) lookup and uniqueness
    std::unordered_map<std::string,int> vars; // numeric variables (HP, SAN, counters...)
};
static GameState g_state;
static std::string g_saveError;

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

static void SaveExploreState()
{
    std::string path = GetExploreSavePath();
    json j;
    j["currentNode"] = g_state.currentNode;
    j["flags"] = json::array();
    for (auto &f : g_state.flags) j["flags"].push_back(f);
    j["inventory"] = json::array();
    for (auto &it : g_state.inventory) j["inventory"].push_back(it);
    j["vars"] = json::object();
    for (auto &kv : g_state.vars) j["vars"][kv.first] = kv.second;
    // inventory future
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
        g_saveError.clear();
        ::Log::Log(::Log::Level::Info, std::string("Explore: loaded save from: ") + path);
    } catch (const std::exception &ex) {
        g_saveError = std::string("Load failed: ") + ex.what();
        ::Log::Log(::Log::Level::Error, g_saveError);
    }
}
// pending transition state so the UI / dice visual have time to play
static bool g_waitingChoice = false;
static int g_pendingNode = -1;
static float g_pendingTimer = 0.0f;

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
    // show choices vertically and support pending transition so user sees dice visual
    auto ApplyFlags = [&](const std::vector<std::string> &sets, const std::vector<std::string> &clears) {
        for (auto &f : sets) g_state.flags.insert(f);
        for (auto &f : clears) g_state.flags.erase(f);
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
                    g_pendingNode = (c.nextOnSuccess >= 0) ? c.nextOnSuccess : -1;
                } else {
                    ApplyFlags(c.setOnFail, c.clearOnFail);
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


