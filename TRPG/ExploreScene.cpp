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
#include <filesystem>
#include <Windows.h>
#include "SceneManager.h"

using json = nlohmann::json;

static std::unordered_map<int, EventNode> g_nodes;
static int g_currentNode = -1;
static std::vector<std::string> g_log;
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
    g_currentNode = -1;
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
        g_currentNode = -1;
    } else {
        if (g_nodes.count(0)) g_currentNode = 0;
        else {
            // pick first available node id
            g_currentNode = g_nodes.begin()->first;
            g_log.push_back(std::string("Explore: start node 0 not found, using node ") + std::to_string(g_currentNode));
        }
        g_log.push_back(std::string("Explore: loaded nodes: ") + std::to_string((int)g_nodes.size()));
        ::Log::Log(::Log::Level::Info, std::string("ExploreScene: loaded nodes from: ") + path + ", count=" + std::to_string((int)g_nodes.size()));
    }
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
                if (g_pendingNode >= 0 && g_nodes.count(g_pendingNode)) g_currentNode = g_pendingNode;
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
    ImGui::Text("Current node: %d", g_currentNode);
    if (ImGui::Button("Log Clear")) g_log.clear();
    ImGui::Separator();
    ImGui::BeginChild("Log", ImVec2(0,200), true);
    for (auto &s : g_log) ImGui::TextWrapped("%s", s.c_str());
    ImGui::EndChild();
    ImGui::End();
}

void ExploreScene::Render() {
    if (g_currentNode < 0 || g_nodes.find(g_currentNode) == g_nodes.end()) {
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
    const EventNode &n = g_nodes[g_currentNode];

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
    for (size_t i = 0; i < n.choices.size(); ++i) {
        const Choice &c = n.choices[i];
        ImGui::PushID((int)i);
        if (!c.rollCond.has_value()) {
            if (ImGui::Button(c.text.c_str(), ImVec2(-1, 0))) {
                g_log.push_back(std::string("Choice: ") + c.text);
                if (c.nextNodeID >= 0 && g_nodes.count(c.nextNodeID)) {
                    // small delay so player sees UI feedback
                    g_pendingNode = c.nextNodeID;
                    g_pendingTimer = 0.18f;
                    g_waitingChoice = true;
                } else g_log.push_back("Choice leads nowhere.");
            }
        } else {
            // roll button shows condition
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s (Roll %d)", c.text.c_str(), c.rollCond->sides);
            if (ImGui::Button(buf, ImVec2(-1, 0))) {
                // perform roll (no immediate node change)
                int r = Dice::RollDieNoVisual(c.rollCond->sides);
                // visual
                std::vector<int> faces = { r };
                DiceVisual::Instance().StartRollFaces(c.rollCond->sides, faces);
                bool success = c.rollCond->greaterOrEqual ? (r >= c.rollCond->threshold) : (r <= c.rollCond->threshold);
                std::string msg = std::string("Rolled: ") + std::to_string(r) + (success ? " (SUCCESS)" : " (FAIL)");
                g_log.push_back(msg);
                // set pending node depending on result, allow visual to play for 1.0s
                if (success) g_pendingNode = (c.nextOnSuccess >= 0) ? c.nextOnSuccess : -1;
                else g_pendingNode = (c.nextOnFail >= 0) ? c.nextOnFail : -1;
                g_pendingTimer = 1.0f; // wait for dice to settle / display
                g_waitingChoice = true;
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


