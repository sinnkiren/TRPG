#include "StoryPlayer.h"
#include "system/imgui/imgui.h"
#include "ImGuiHelpers.h"
#include "system/json.hpp"
#include "TextureManager.h"
#include "EventNode.h"
#include "Logging.h"
#include <fstream>
#include "AssetManager.h"
#include "SceneManager.h"
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

namespace {
// utility: split comma-separated to vector<string>
static std::vector<std::string> SplitList(const std::string &s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, ',')) {
        // trim
        auto l = item.find_first_not_of(" \t\n\r");
        auto r = item.find_last_not_of(" \t\n\r");
        if (l==std::string::npos) continue;
        out.push_back(item.substr(l, r-l+1));
    }
    return out;
}

static std::string JoinList(const std::vector<std::string> &v) {
    std::string s;
    for (size_t i=0;i<v.size();++i) {
        if (i) s += ",";
        s += v[i];
    }
    return s;
}

// join/unjoin int map as key=val,key2=val2
static std::string JoinIntMap(const std::unordered_map<std::string,int> &m) {
    std::string s;
    bool first = true;
    for (auto &kv : m) {
        if (!first) s += ",";
        s += kv.first + "=" + std::to_string(kv.second);
        first = false;
    }
    return s;
}

static std::unordered_map<std::string,int> ParseIntMap(const std::string &str) {
    std::unordered_map<std::string,int> out;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        auto eq = token.find('=');
        if (eq == std::string::npos) continue;
        auto k = token.substr(0, eq);
        auto v = token.substr(eq+1);
        // trim
        auto l = k.find_first_not_of(" \t\n\r");
        auto r = k.find_last_not_of(" \t\n\r");
        if (l==std::string::npos) continue;
        k = k.substr(l, r-l+1);
        l = v.find_first_not_of(" \t\n\r");
        r = v.find_last_not_of(" \t\n\r");
        if (l==std::string::npos) continue;
        v = v.substr(l, r-l+1);
        try {
            int vi = std::stoi(v);
            out[k] = vi;
        } catch (...) {
            // ignore
        }
    }
    return out;
}
}
// parse/draw characters implementations moved to StoryPlayer_Editor_Utils.cpp
// Character parse/draw functions implemented in StoryPlayer_Editor_Utils.cpp
void ParseCharactersJson(const json &cj, std::vector<CharacterState> &out);
void DrawCharacters(const std::vector<CharacterState>& chars, ImDrawList* bg, const ImGuiViewport* vp);

// Node/editor methods
int StoryPlayer::CreateNode()
{
    int maxid = -1;
    for (auto &p : m_nodeMap) maxid = std::max(maxid, p.first);
    int nid = maxid + 1;
    EventNode n; n.id = nid; n.text = "New node";
    m_nodeMap[nid] = n;
    return nid;
}

bool StoryPlayer::DeleteNode(int nodeId)
{
    auto it = m_nodeMap.find(nodeId);
    if (it == m_nodeMap.end()) return false;
    m_nodeMap.erase(it);
    for (auto &p : m_nodeMap) {
        auto &choices = p.second.choices;
        for (auto itc = choices.begin(); itc != choices.end(); ) {
            if (itc->nextNodeID == nodeId) itc = choices.erase(itc);
            else ++itc;
        }
    }
    return true;
}

bool StoryPlayer::AddChoiceToNode(int nodeId, const Choice& choice)
{
    auto it = m_nodeMap.find(nodeId);
    if (it == m_nodeMap.end()) return false;
    it->second.choices.push_back(choice);
    return true;
}

bool StoryPlayer::RemoveChoiceFromNode(int nodeId, int choiceIndex)
{
    auto it = m_nodeMap.find(nodeId);
    if (it == m_nodeMap.end()) return false;
    auto &choices = it->second.choices;
    if (choiceIndex < 0 || choiceIndex >= (int)choices.size()) return false;
    choices.erase(choices.begin() + choiceIndex);
    return true;
}

bool StoryPlayer::SaveGraphToFile(const std::string& path) const
{
    try {
        json out;
        out["nodes"] = json::array();
        for (const auto &p : m_nodeMap) {
            const EventNode &n = p.second;
            json nj;
            nj["id"] = n.id;
            nj["text"] = n.text;
            if (!n.choices.empty()) {
                nj["choices"] = json::array();
                for (const auto &c : n.choices) {
                    json cj;
                    cj["text"] = c.text;
                    cj["next"] = c.nextNodeID;
                    if (c.rollCond.has_value()) {
                        cj["roll"] = json::object();
                        cj["roll"]["sides"] = c.rollCond->sides;
                        cj["roll"]["threshold"] = c.rollCond->threshold;
                        cj["roll"]["greaterOrEqual"] = c.rollCond->greaterOrEqual;
                    }
                    nj["choices"].push_back(cj);
                }
            }
            // ノード位置が保存されていれば pos オブジェクトとして出力する
            auto itpos = m_nodePositions.find(n.id);
            if (itpos != m_nodePositions.end()) {
                nj["pos"] = { {"x", itpos->second.x}, {"y", itpos->second.y} };
            }
            out["nodes"].push_back(nj);
        }
        if (!m_characters.empty()) {
            out["characters"] = json::array();
            for (const auto &c : m_characters) {
                json cj;
                cj["id"] = c.id;
                cj["image"] = c.imagePath;
                cj["expression"] = c.expression;
                cj["visible"] = c.visible;
                cj["position"] = { {"x", c.position.x}, {"y", c.position.y} };
                out["characters"].push_back(cj);
            }
        }

        std::ofstream ofs(path);
        if (!ofs.is_open()) return false;
        ofs << out.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

void StoryPlayer::RenderEditorUI()
{
    // Editor only available in dev mode
    if (!g_SceneManager.IsDevMode()) return;
    if (ImGui::GetCurrentContext() == nullptr) return;

    ImGui::Begin("Story Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    // Path input (default to asset path on first use)
    if (m_editorStoryPath.empty()) m_editorStoryPath = AssetManager::GetStoryPath("story.json");
    char pathBuf[1024] = {0};
    strncpy_s(pathBuf, m_editorStoryPath.c_str(), sizeof(pathBuf)-1);
    if (ImGui::InputText("Story Path", pathBuf, sizeof(pathBuf))) {
        m_editorStoryPath = std::string(pathBuf);
    }

    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        if (!LoadFromFile(m_editorStoryPath)) {
            // m_lastLoadError set by LoadFromFile
        } else {
            m_editorSelected = (m_events.empty() ? -1 : 0);
            m_editorEffectParamsBuf.clear();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (!SaveToFile(m_editorStoryPath)) {
            m_lastLoadError = "Failed to save to: " + m_editorStoryPath;
        }
    }

    ImGui::Separator();
    // Editor mode selection
    const char* modes[] = { "Events", "Node Graph" };
    ImGui::Combo("Editor Mode", &m_editorMode, modes, IM_ARRAYSIZE(modes));

    if (m_editorMode == 0) RenderEventEditor();
    else {
        RenderNodeEditor();
        // Graph canvas drawn as part of node editor
        RenderNodeGraphCanvas();
    }

    ImGui::Separator();
    RenderSelectedEventEditor();

    if (!m_lastLoadError.empty()) ImGui::TextWrapped("Error: %s", m_lastLoadError.c_str());

    ImGui::End();
}

// --- Refactored editor sub-routines ---
void StoryPlayer::RenderEventEditor()
{
    // Event list and basic controls (New/Delete/Up/Down)
    ImGui::Text("Events: %d", (int)m_events.size());
    ImGui::BeginChild("EventList", ImVec2(300,200), true);
    for (int i=0;i<(int)m_events.size();++i) {
        const StoryEvent& ev = m_events[i];
        char label[256];
        std::string shortText = ev.text;
        if (shortText.size() > 60) shortText = shortText.substr(0,60) + "...";
        snprintf(label, sizeof(label), "%03d: %s", i, shortText.c_str());
        if (ImGui::Selectable(label, m_editorSelected==i)) {
            m_editorSelected = i;
            if (!m_events[i].effectParams.is_null()) m_editorEffectParamsBuf = m_events[i].effectParams.dump(2);
            else m_editorEffectParamsBuf.clear();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImGui::Button("New")) {
        m_events.emplace_back();
        m_editorSelected = (int)m_events.size()-1;
        m_editorEffectParamsBuf.clear();
    }
    if (m_editorSelected >= 0 && m_editorSelected < (int)m_events.size()) {
        if (ImGui::Button("Delete")) {
            m_events.erase(m_events.begin() + m_editorSelected);
            m_editorSelected = std::min((int)m_editorSelected, (int)m_events.size()-1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Up") && m_editorSelected > 0) {
            std::swap(m_events[m_editorSelected], m_events[m_editorSelected-1]);
            m_editorSelected--;
        }
        ImGui::SameLine();
        if (ImGui::Button("Down") && m_editorSelected+1 < (int)m_events.size()) {
            std::swap(m_events[m_editorSelected], m_events[m_editorSelected+1]);
            m_editorSelected++;
        }
    }
    ImGui::EndGroup();
}

void StoryPlayer::RenderSelectedEventEditor()
{
    if (m_editorSelected >= 0 && m_editorSelected < (int)m_events.size()) {
        StoryEvent& ev = m_events[m_editorSelected];
        char buf[512];
        strncpy_s(buf, ev.speaking.c_str(), sizeof(buf)-1);
        if (ImGui::InputText("Speaker", buf, sizeof(buf))) ev.speaking = std::string(buf);
        strncpy_s(buf, ev.faceImage.c_str(), sizeof(buf)-1);
        if (ImGui::InputText("Face", buf, sizeof(buf))) ev.faceImage = std::string(buf);
        strncpy_s(buf, ev.effect.c_str(), sizeof(buf)-1);
        if (ImGui::InputText("Effect", buf, sizeof(buf))) ev.effect = std::string(buf);
        float dur = ev.duration;
        if (ImGui::InputFloat("Duration", &dur)) ev.duration = dur;

        // Safe multiline input for event text
        ImGui_InputTextMultiline_String("Text", ev.text, 1024, ImVec2(400,120));

        // Safe multiline input for effectParams (JSON)
        ImGui_InputTextMultiline_String("effectParams (JSON)", m_editorEffectParamsBuf, 2048, ImVec2(400,100));

        if (ImGui::Button("Apply effectParams")) {
            try {
                if (m_editorEffectParamsBuf.empty()) ev.effectParams = nullptr;
                else ev.effectParams = json::parse(m_editorEffectParamsBuf);
                m_lastLoadError.clear();
            }
            catch (const std::exception& ex) {
                m_lastLoadError = std::string("effectParams JSON parse error: ") + ex.what();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Preview")) {
            PreviewEvent(m_editorSelected);
        }
    }
}

void StoryPlayer::RenderNodeEditor()
{
    // Node list and controls
    ImGui::BeginChild("NodeList", ImVec2(300,300), true);
    ImGui::Text("Nodes: %d", (int)m_nodeMap.size());
    for (auto &p : m_nodeMap) {
        int id = p.first;
        std::string label = std::to_string(id) + ": " + (p.second.text.size() > 40 ? p.second.text.substr(0,40)+"..." : p.second.text);
        if (ImGui::Selectable(label.c_str(), m_nodeEditorSelectedId==id)) {
            m_nodeEditorSelectedId = id;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImGui::Button("New Node")) {
        int nid = CreateNode();
        m_nodeEditorSelectedId = nid;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Graph")) {
        if (!SaveGraphToFile(m_editorStoryPath)) {
            m_lastLoadError = "Failed to save graph to: " + m_editorStoryPath;
        }
    }

    ImGui::Separator();
    if (m_nodeEditorSelectedId >= 0) {
        auto it = m_nodeMap.find(m_nodeEditorSelectedId);
        if (it != m_nodeMap.end()) {
            EventNode &node = it->second;
            // Safe multiline input for node text
            ImGui_InputTextMultiline_String("Node Text", node.text, 1024, ImVec2(400,120));
            ImGui::Separator();
            ImGui::Text("Choices (%d)", (int)node.choices.size());
            for (int i=0;i<(int)node.choices.size();++i) {
                RenderChoiceEditor(node, i);
            }
            if (ImGui::Button("Add Choice")) {
                Choice nc; nc.text = "New choice"; nc.nextNodeID = -1; node.choices.push_back(nc);
            }
            ImGui::Separator();
            if (ImGui::Button("Delete Node")) {
                DeleteNode(m_nodeEditorSelectedId);
                m_nodeEditorSelectedId = -1;
            }
        }
    }
    ImGui::EndGroup();
}

void StoryPlayer::RenderChoiceEditor(EventNode &node, int i)
{
    Choice &c = node.choices[i];
    char bufc[256];
    strncpy_s(bufc, c.text.c_str(), sizeof(bufc)-1);
    if (ImGui::InputText(std::string("ChoiceText##"+std::to_string(i)).c_str(), bufc, sizeof(bufc))) {
        c.text = std::string(bufc);
    }
    int nid = c.nextNodeID;
    if (ImGui::InputInt(std::string("NextNode##"+std::to_string(i)).c_str(), &nid)) c.nextNodeID = nid;
    ImGui::SameLine();
    if (ImGui::Button(std::string("Delete Choice##"+std::to_string(i)).c_str())) {
        RemoveChoiceFromNode(m_nodeEditorSelectedId, i);
        return; // adjust loop
    }

    // Roll settings
    if (ImGui::CollapsingHeader((std::string("Roll Settings##") + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        bool hasRoll = c.rollCond.has_value();
        if (ImGui::Checkbox((std::string("Use Roll##hasroll") + std::to_string(i)).c_str(), &hasRoll)) {
            if (hasRoll) c.rollCond = RollCond(); else c.rollCond.reset();
        }
        if (c.rollCond.has_value()) {
            int sides = c.rollCond->sides;
            int threshold = c.rollCond->threshold;
            bool ge = c.rollCond->greaterOrEqual;
            if (ImGui::InputInt((std::string("Sides##") + std::to_string(i)).c_str(), &sides)) c.rollCond->sides = sides;
            if (ImGui::InputInt((std::string("Threshold##") + std::to_string(i)).c_str(), &threshold)) c.rollCond->threshold = threshold;
            if (ImGui::Checkbox((std::string("GreaterOrEqual##") + std::to_string(i)).c_str(), &ge)) c.rollCond->greaterOrEqual = ge;
            int ns = c.nextOnSuccess; int nf = c.nextOnFail;
            if (ImGui::InputInt((std::string("NextOnSuccess##") + std::to_string(i)).c_str(), &ns)) c.nextOnSuccess = ns;
            if (ImGui::InputInt((std::string("NextOnFail##") + std::to_string(i)).c_str(), &nf)) c.nextOnFail = nf;
        }
    }

    // Effects editor
    if (ImGui::CollapsingHeader((std::string("Effects##") + std::to_string(i)).c_str())) {
        RenderEffectsEditor(c, i);
    }
}

void StoryPlayer::RenderEffectsEditor(Choice &c, int i)
{
    // list existing effects
    for (int ei=0; ei<(int)c.effects.size(); ++ei) {
        Choice::Effect &ef = c.effects[ei];
        ImGui::PushID((std::string("effect") + std::to_string(i) + "_" + std::to_string(ei)).c_str());
        // op selector
        const char* ops[] = { "set_flag", "clear_flag", "add_inventory", "remove_inventory", "add_var", "set_var" };
        int opIdx = 0;
        std::string op = ef.op;
        for (int oi=0; oi< (int)(sizeof(ops)/sizeof(ops[0])); ++oi) if (op == ops[oi]) opIdx = oi;
        if (ImGui::Combo("Op", &opIdx, ops, IM_ARRAYSIZE(ops))) ef.op = ops[opIdx];

        if (ef.op == "set_flag" || ef.op == "clear_flag" || ef.op == "add_inventory" || ef.op == "remove_inventory") {
            // items list as comma-separated
            std::string items = JoinList(ef.items);
            char bufItems[1024]; strncpy_s(bufItems, items.c_str(), sizeof(bufItems)-1);
            if (ImGui::InputText("Items (comma-separated)", bufItems, sizeof(bufItems))) {
                ef.items = SplitList(std::string(bufItems));
            }
        } else if (ef.op == "add_var" || ef.op == "set_var") {
            char keybuf[256]; strncpy_s(keybuf, ef.key.c_str(), sizeof(keybuf)-1);
            if (ImGui::InputText("Key", keybuf, sizeof(keybuf))) ef.key = std::string(keybuf);
            int v = ef.intValue;
            if (ImGui::InputInt("Value", &v)) ef.intValue = v;
        }

        if (ImGui::Button("Delete Effect")) { c.effects.erase(c.effects.begin()+ei); ImGui::PopID(); break; }
        ImGui::PopID();
    }
    if (ImGui::Button((std::string("Add Effect##") + std::to_string(i)).c_str())) {
        Choice::Effect ne; ne.op = "set_flag"; ne.items.push_back("flag_name"); c.effects.push_back(ne);
    }
    // editor for roll success/fail extra effects
    if (c.rollCond.has_value()) {
        if (ImGui::TreeNode((std::string("On Success Effects##") + std::to_string(i)).c_str())) {
            for (int ei=0; ei<(int)c.effectsOnSuccess.size(); ++ei) {
                Choice::Effect &ef = c.effectsOnSuccess[ei];
                ImGui::PushID((std::string("suceff") + std::to_string(i) + "_" + std::to_string(ei)).c_str());
                char bufItems[1024]; strncpy_s(bufItems, JoinList(ef.items).c_str(), sizeof(bufItems)-1);
                if (ImGui::InputText("Items", bufItems, sizeof(bufItems))) ef.items = SplitList(std::string(bufItems));
                if (ImGui::Button("Delete")) { c.effectsOnSuccess.erase(c.effectsOnSuccess.begin()+ei); ImGui::PopID(); break; }
                ImGui::PopID();
            }
            if (ImGui::Button((std::string("Add Success Effect##") + std::to_string(i)).c_str())) { Choice::Effect e; e.op = "set_flag"; e.items.push_back("flag"); c.effectsOnSuccess.push_back(e); }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode((std::string("On Fail Effects##") + std::to_string(i)).c_str())) {
            for (int ei=0; ei<(int)c.effectsOnFail.size(); ++ei) {
                Choice::Effect &ef = c.effectsOnFail[ei];
                ImGui::PushID((std::string("faileff") + std::to_string(i) + "_" + std::to_string(ei)).c_str());
                char bufItems[1024]; strncpy_s(bufItems, JoinList(ef.items).c_str(), sizeof(bufItems)-1);
                if (ImGui::InputText("Items", bufItems, sizeof(bufItems))) ef.items = SplitList(std::string(bufItems));
                if (ImGui::Button("Delete")) { c.effectsOnFail.erase(c.effectsOnFail.begin()+ei); ImGui::PopID(); break; }
                ImGui::PopID();
            }
            if (ImGui::Button((std::string("Add Fail Effect##") + std::to_string(i)).c_str())) { Choice::Effect e; e.op = "set_flag"; e.items.push_back("flag"); c.effectsOnFail.push_back(e); }
            ImGui::TreePop();
        }
    }
}

// RenderNodeGraphCanvas moved to StoryPlayer_NodeGraph.cpp
