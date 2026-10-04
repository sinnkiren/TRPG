#include "Application.h"
#include "system/stb_image.h"
#include "StoryPlayer.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Logging.h"
#include <fstream>
#include "system/imgui/imgui.h"
#include <random>
#include "system/json.hpp" 
#include "FearEffects.h"
#include "AssetManager.h"
#include <filesystem>
#include <d3d11.h>
#include <direct.h> // _getcwd
#include "EventNode.h"

using json = nlohmann::json;

namespace {
#ifdef _WIN32
std::filesystem::path Utf8ToFilesystemPath(const std::string& s)
{
    if (s.empty()) return std::filesystem::path();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return std::filesystem::path(s);
    std::wstring ws(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), len);
    return std::filesystem::path(ws);
}
#else
std::filesystem::path Utf8ToFilesystemPath(const std::string& s)
{
    return std::filesystem::path(s);
}
#endif
}

// Implementations moved to StoryPlayer_Editor.cpp

// (Moved runtime node UI and character drawing helpers to StoryPlayer_Runtime.cpp)



void StoryPlayer::AddOrUpdateCharacter(const CharacterState& s)
{
    for (auto &c : m_characters) {
        if (c.id == s.id) { c = s; return; }
    }
    m_characters.push_back(s);
}

void StoryPlayer::LoadCharacterTexturesIfNeeded()
{
    for (auto &c : m_characters) {
        if (c.tex) continue;
        if (c.imagePath.empty()) continue;
#ifdef IMGUI_IMPL_DIRECTX11
        ImTextureID tid = TextureManager::GetImGuiTextureID(c.imagePath);
        if (tid) c.tex = tid;
#else
        ImTextureID tid = TextureManager::GetImGuiTexture(c.imagePath);
        if (tid) c.tex = tid;
#endif
    }
}

bool StoryPlayer::RemoveCharacterById(const std::string& id)
{
    for (size_t i=0;i<m_characters.size();++i) {
        if (m_characters[i].id == id) { m_characters.erase(m_characters.begin()+i); return true; }
    }
    return false;
}

CharacterState* StoryPlayer::FindCharacter(const std::string& id)
{
    for (auto &c : m_characters) if (c.id == id) return &c;
    return nullptr;
}

void StoryPlayer::SetCharacterVisible(const std::string& id, bool visible)
{
    CharacterState* c = FindCharacter(id);
    if (c) c->visible = visible;
}


StoryPlayer::StoryPlayer() {}
StoryPlayer::~StoryPlayer() {
#ifdef IMGUI_IMPL_DIRECTX11
    if (m_bgSrv) { m_bgSrv->Release(); m_bgSrv = nullptr; }
#endif
}

bool StoryPlayer::LoadGraphFromFile(const std::string& path)
{
    m_lastLoadError.clear();
    namespace fs = std::filesystem;
    try {
        // Construct a filesystem path from UTF-8 input to avoid ANSI code-page conversions on Windows.
        fs::path ppath = Utf8ToFilesystemPath(path);
        if (!fs::exists(ppath)) {
            m_lastLoadError = "Graph file does not exist: " + path;
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        // Open using fs::path so the implementation can use wide APIs on Windows
        std::ifstream ifs(ppath, std::ios::binary);
        if (!ifs.is_open()) {
            m_lastLoadError = "Failed to open graph file: " + path;
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        json j;
        try {
            ifs >> j;
        } catch (const std::exception& ex) {
            m_lastLoadError = std::string("Failed to parse graph JSON: ") + ex.what();
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        // Continue using 'j' below (move on to parsing nodes)

        // Support two formats:
        // 1) legacy: root is an array of nodes
        // 2) object: { "nodes": [...], "characters": [...] }
        json nodesArray;
        if (j.is_array()) {
            nodesArray = j;
        } else if (j.is_object() && j.contains("nodes") && j["nodes"].is_array()) {
            nodesArray = j["nodes"];
            if (j.contains("characters")) {
                try {
                    ParseCharactersJson(j["characters"], m_characters);
                } catch (...) {
                    ::Log::Log(::Log::Level::Warning, "StoryPlayer: failed to parse characters array in graph JSON");
                }
            }
        } else {
            m_lastLoadError = "Graph JSON root is not an array or object with 'nodes': " + path;
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        m_nodeMap.clear();
        try {
            for (auto &nj : nodesArray) {
                EventNode n = EventNode::FromJson(nj);
                if (n.id < 0) continue;
                m_nodeMap[n.id] = std::move(n);
            }
        } catch (const std::exception& ex) {
            m_lastLoadError = std::string("Error reading graph entries: ") + ex.what();
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            m_nodeMap.clear();
            return false;
        }

        // set start node to smallest id
        if (!m_nodeMap.empty()) {
            int start = m_nodeMap.begin()->first;
            for (auto &p : m_nodeMap) start = std::min(start, p.first);
            m_currentNodeId = start;
            m_usingNodeGraph = true;
            m_timer = 0.0f;
            m_playing = false;
        }

        m_lastLoadError.clear();
        return true;

    } catch (const std::exception& ex) {
        m_lastLoadError = std::string("Filesystem check failed: ") + ex.what();
        ::Log::Log(::Log::Level::Error, m_lastLoadError);
        return false;
    }


}

void StoryPlayer::SelectChoice(int choiceIndex)
{
    if (!m_usingNodeGraph) return;
    auto it = m_nodeMap.find(m_currentNodeId);
    if (it == m_nodeMap.end()) return;
    EventNode &node = it->second;
    if (choiceIndex < 0 || choiceIndex >= (int)node.choices.size()) return;
    const Choice &c = node.choices[choiceIndex];
    // Check requirements first (flags, inventory, vars)
    auto CheckRequirements = [&](const Choice &ch)->bool {
        for (auto &f : ch.requireFlags) if (m_flags.find(f) == m_flags.end()) return false;
        for (auto &f : ch.requireNotFlags) if (m_flags.find(f) != m_flags.end()) return false;
        for (auto &it : ch.requireInventory) {
            bool found = false;
            for (auto &inv : m_inventory) if (inv == it) { found = true; break; }
            if (!found) return false;
        }
        for (auto &kv : ch.requireVarMin) {
            int v = 0; auto itv = m_vars.find(kv.first); if (itv != m_vars.end()) v = itv->second;
            if (v < kv.second) return false;
        }
        for (auto &kv : ch.requireVarMax) {
            int v = 0; auto itv = m_vars.find(kv.first); if (itv != m_vars.end()) v = itv->second;
            if (v > kv.second) return false;
        }
        return true;
    };

    if (!CheckRequirements(c)) {
        ::Log::Log(::Log::Level::Warning, "StoryPlayer::SelectChoice - choice requirements not met");
        return;
    }

    // Helper to apply a list of effects
    std::function<void(const std::vector<Choice::Effect>&)> ApplyEffects;
    // condition evaluator
    auto EvalCondition = [&](const Choice::Effect::Condition &cond)->bool {
        int passCount = 0, total = 0;
        auto checkFlag = [&](const std::string &f)->bool { return m_flags.find(f) != m_flags.end(); };
        for (auto &f : cond.requireFlags) { total++; if (checkFlag(f)) passCount++; }
        for (auto &f : cond.requireNotFlags) { total++; if (!checkFlag(f)) passCount++; }
        for (auto &it : cond.requireInventory) { total++; bool found=false; for (auto &inv : m_inventory) if (inv==it) { found=true; break; } if (found) passCount++; }
        for (auto &kv : cond.requireVarMin) { total++; int v=0; auto itv=m_vars.find(kv.first); if (itv!=m_vars.end()) v=itv->second; if (v>=kv.second) passCount++; }
        for (auto &kv : cond.requireVarMax) { total++; int v=0; auto itv=m_vars.find(kv.first); if (itv!=m_vars.end()) v=itv->second; if (v<=kv.second) passCount++; }
        if (total == 0) return true;
        if (cond.mode == Choice::Effect::Condition::Mode::ALL) return passCount == total;
        return passCount > 0;
    };

    ApplyEffects = [&](const std::vector<Choice::Effect> &effs) {
        for (const auto &ef : effs) {
            if (ef.op == "set_flag") {
                for (auto &it : ef.items) m_flags.insert(it);
            } else if (ef.op == "clear_flag") {
                for (auto &it : ef.items) m_flags.erase(it);
            } else if (ef.op == "add_inventory") {
                for (auto &it : ef.items) m_inventory.push_back(it);
            } else if (ef.op == "remove_inventory") {
                for (auto &it : ef.items) {
                    for (auto invIt = m_inventory.begin(); invIt != m_inventory.end(); ) {
                        if (*invIt == it) { invIt = m_inventory.erase(invIt); break; }
                        else ++invIt;
                    }
                }
            } else if (ef.op == "add_var") {
                if (!ef.key.empty()) m_vars[ef.key] += ef.intValue;
            } else if (ef.op == "set_var") {
                if (!ef.key.empty()) m_vars[ef.key] = ef.intValue;
            } else if (ef.op == "if") {
                bool c = EvalCondition(ef.condition);
                if (c) ApplyEffects(ef.thenEffects); else ApplyEffects(ef.elseEffects);
            } else {
                // unknown op: try legacy single-item ops
                if (ef.op == "set_flags") { for (auto &it: ef.items) m_flags.insert(it); }
            }
        }
    };

    // Apply base effects first
    ApplyEffects(c.effects);

    // Determine next node and apply roll branching if present
    int nextId = c.nextNodeID;
    if (c.rollCond.has_value()) {
        // Use deterministic RNG during preview
        std::mt19937 localRng = m_previewActive ? std::mt19937(123456) : FearEffects::GetRng();
        int sides = std::max(1, c.rollCond->sides);
        std::uniform_int_distribution<int> dist(1, sides);
        int r = dist(localRng);
        bool success = c.rollCond->greaterOrEqual ? (r >= c.rollCond->threshold) : (r <= c.rollCond->threshold);
        ::Log::Log(::Log::Level::Info, std::string("StoryPlayer: roll result = ") + std::to_string(r) + (success?" (success)":" (fail)"));
        if (success) {
            ApplyEffects(c.effectsOnSuccess);
            if (c.nextOnSuccess >= 0) nextId = c.nextOnSuccess;
        } else {
            ApplyEffects(c.effectsOnFail);
            if (c.nextOnFail >= 0) nextId = c.nextOnFail;
        }
    }

    if (nextId >= 0 && m_nodeMap.find(nextId) != m_nodeMap.end()) {
        m_currentNodeId = nextId;
        m_timer = 0.0f;
        ShowCurrentText();
    } else {
        // no next: end graph
        m_usingNodeGraph = false;
    }
}

void StoryPlayer::PreviewEvent(int index)
{
    if (index < 0 || index >= (int)m_events.size()) return;
    // If already previewing, stop previous
    if (m_previewActive) StopPreview();
    // save state
    m_previewPrevIndex = m_index;
    m_previewPrevPlaying = m_playing;
    m_previewPrevTimer = m_timer;
    // set preview state
    m_index = index;
    m_timer = 0.0f;
    m_playing = false; // don't affect main playback
    m_previewElapsed = 0.0f;
    m_previewDuration = m_events[index].duration;
    m_previewActive = true;
    // Trigger effect for preview
    TriggerEffect(m_events[index]);
    ShowCurrentText();
}

void StoryPlayer::StopPreview()
{
    if (!m_previewActive) return;
    // restore previous state
    m_index = m_previewPrevIndex;
    m_playing = m_previewPrevPlaying;
    m_timer = m_previewPrevTimer;
    m_previewActive = false;
    m_previewElapsed = 0.0f;
    m_previewDuration = 0.0f;
}

void StoryPlayer::RenderDevPanelContents()
{
    ImGui::Text("StoryPlayer Dev Info");
    ImGui::Separator();
    ImGui::Text("BG Path: %s", m_bgPath.empty() ? "(empty)" : m_bgPath.c_str());
    ImGui::Text("Load attempts: %d", m_bgLoadAttempts);
    ImGui::Text("Loaded attempted: %s", m_bgLoadedAttempted ? "yes" : "no");
    if (!m_lastLoadError.empty()) ImGui::TextWrapped("Last load error: %s", m_lastLoadError.c_str());
    ImGui::Separator();
    // Node info: current index and total
    ImGui::Text("Current node: %d / %d", m_index, (int)m_events.size());

    // Jump controls
    ImGui::InputInt("Jump to node", &m_devNodeInput);
    ImGui::SameLine();
    if (ImGui::Button("Go")) {
        if (m_devNodeInput >= 0 && m_devNodeInput < (int)m_events.size()) {
            m_index = m_devNodeInput;
            m_timer = 0.0f;
            ShowCurrentText();
            ::Log::Log(::Log::Level::Info, std::string("StoryPlayer: jumped to node ") + std::to_string(m_devNodeInput));
        }
        else {
            ::Log::Log(::Log::Level::Warning, std::string("StoryPlayer: invalid node id for jump: ") + std::to_string(m_devNodeInput));
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Next Node")) {
        Next();
    }
}

void StoryPlayer::RenderUI()
{
    // Dev-only: show background load status window, toggleable
    if (!g_SceneManager.IsDevMode()) return;
    if (ImGui::GetCurrentContext() == nullptr) return;
    ImGui::Begin("StoryPlayer Dev", &m_showDevWindow, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("BG Path: %s", m_bgPath.empty() ? "(empty)" : m_bgPath.c_str());
    ImGui::Text("Loaded attempted: %s", m_bgLoadedAttempted ? "yes" : "no");
    ImGui::Text("BG Tex set: %s", m_bgTex ? "yes" : "no");
    if (!m_lastLoadError.empty()) ImGui::TextWrapped("Last load error: %s", m_lastLoadError.c_str());
    ImGui::Separator();
    if (ImGui::Button(m_bgLoadedAttempted ? "Retry Load" : "Load Background")) {
        // Reset attempts so LoadBackgroundTextureIfNeeded will try immediately
        m_bgLoadedAttempted = false;
        m_bgLoadAttempts = 0;
        m_bgLastAttemptTime = 0.0f;
        LoadBackgroundTextureIfNeeded();
    }
    ImGui::End();

    // Quick controls for node-graph mode
    if (m_usingNodeGraph) {
        ImGui::Begin("Node Graph Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Current Node: %d", m_currentNodeId);
        if (ImGui::Button("Restart Graph")) {
            if (!m_nodeMap.empty()) {
                // pick smallest id as start
                int start = m_nodeMap.begin()->first;
                for (auto &p : m_nodeMap) start = std::min(start, p.first);
                m_currentNodeId = start;
                m_timer = 0.0f;
                ShowCurrentText();
            }
        }
        ImGui::End();
    }

    // Note: Story Editor UI moved to StoryPlayer::RenderEditorUI and should be hosted by a dedicated scene (StoryEditorScene)
}

// RenderEditorUI implemented in StoryPlayer_Editor.cpp

#ifdef IMGUI_IMPL_DIRECTX11
void StoryPlayer::SetBackgroundSRV(ID3D11ShaderResourceView* srv)
{
    if (m_bgSrv == srv) return;
    // AddRef new SRV first to ensure we own a reference, then release previous
    if (srv) srv->AddRef();
    if (m_bgSrv) { m_bgSrv->Release(); m_bgSrv = nullptr; }
    m_bgSrv = srv;
    m_bgTex = reinterpret_cast<ImTextureID>(srv);
}
#endif

// --- 追加: 遅延ロード用ヘルパー ---
// m_bgPath にパスをセットしておけば、Device が準備できた時点でここでロードします。

void StoryPlayer::LoadBackgroundTextureIfNeeded()
{
    if (m_bgPath.empty()) return;

#ifdef IMGUI_IMPL_DIRECTX11
    // If we've succeeded or exhausted attempts, don't try further
    if (m_bgLoadedAttempted) return;

    // Device が準備できているかチェック
    if (!Application::GetDevice()) {
        // Device 未初期化 => 後で試す
        return;
    }

    // Check if we've already exhausted retries
    if (m_bgMaxLoadAttempts >= 0 && m_bgLoadAttempts >= m_bgMaxLoadAttempts) {
        m_bgLoadedAttempted = true; // mark as done
        m_lastLoadError = "Background load: max attempts reached for: " + m_bgPath;
        return;
    }

    float now = static_cast<float>(ImGui::GetTime());
    // If we've attempted recently, wait until retry interval elapsed
    if (m_bgLoadAttempts > 0) {
        float since = now - m_bgLastAttemptTime;
        if (since < m_bgRetryInterval) return; // wait longer
    }

    // Attempt to load
    m_bgLoadAttempts++;
    m_bgLastAttemptTime = now;

    ID3D11ShaderResourceView* srv = TextureManager::LoadTexture(m_bgPath);
    if (!srv) {
        m_lastLoadError = "StoryPlayer: TextureManager failed to load: " + m_bgPath;
        std::string msg = m_lastLoadError + " (attempt " + std::to_string(m_bgLoadAttempts) + ")";
        ::Log::Log(::Log::Level::Warning, msg);
        // Do not set m_bgLoadedAttempted so we can retry later until max attempts
        return;
    }

    // Success: Set texture (SetBackgroundSRV will AddRef the SRV for ownership)
    SetBackgroundSRV(srv); // m_bgSrv にセット（StoryPlayer が解放を行う）
    m_bgLoadedAttempted = true;
    m_lastLoadError.clear();
    ::Log::Log(::Log::Level::Info, std::string("StoryPlayer: background loaded via TextureManager: ") + m_bgPath);
#endif
}

void StoryPlayer::Initialize() {
    // Do not auto-load a fixed story JSON here.
    // Story selection/loading is handled by ScenarioScene -> SceneManager
    // which sets SceneManager::pendingStoryPath and ApplyPendingChange will
    // call LoadFromFile on the newly created StoryPlayer instance.

    // イベント完了時コールバック（シーン切り替え等）
    // battle_start はフェード開始に置き換え
    onEventFinished = [this](const StoryEvent& ev) {
        if (ev.effect == "battle_start") {
            StartFadeToBattle();
        }
    };

    // 揺れエフェクト ("shake")
    RegisterEffect("shake", [](const StoryEvent& ev) {
        float intensity = 10.0f;
        if (!ev.effectParams.is_null() && ev.effectParams.contains("intensity"))
            intensity = ev.effectParams["intensity"].get<float>();
        FearEffects::StartShake(intensity, ev.duration);
    });

    // オーバーレイエフェクト ("overlay")
    RegisterEffect("overlay", [](const StoryEvent& ev) {
        float intensity = 0.8f;
        int stage = 0;
        if (!ev.effectParams.is_null()) {
            if (ev.effectParams.contains("intensity")) intensity = ev.effectParams["intensity"].get<float>();
            if (ev.effectParams.contains("stage")) stage = ev.effectParams["stage"].get<int>();
        }
        FearEffects::StartOverlay(intensity, ev.duration, stage);
    });

    // ノイズ系エフェクト（ストーリーの恐怖演出用）
    RegisterEffect("noise", [](const StoryEvent& ev) {
        float intensity = 0.6f; // overlay 強度（0..1）
        float shakeIntensity = 6.0f; // 揺れの量（ピクセル等）
        int stage = 2;
        if (!ev.effectParams.is_null()) {
            if (ev.effectParams.contains("intensity")) intensity = ev.effectParams["intensity"].get<float>();
            if (ev.effectParams.contains("stage")) stage = ev.effectParams["stage"].get<int>();
            if (ev.effectParams.contains("shake")) shakeIntensity = ev.effectParams["shake"].get<float>();
        }
        FearEffects::StartOverlay(intensity, ev.duration, stage);
        FearEffects::StartShake(shakeIntensity, ev.duration);
    });

    // 血やショッキングな表現（強い赤オーバーレイ＋短い揺れ）
    RegisterEffect("blood", [](const StoryEvent& ev) {
        float intensity = 1.0f;
        float shakeIntensity = 8.0f;
        int stage = 4;
        if (!ev.effectParams.is_null()) {
            if (ev.effectParams.contains("intensity")) intensity = ev.effectParams["intensity"].get<float>();
            if (ev.effectParams.contains("stage")) stage = ev.effectParams["stage"].get<int>();
            if (ev.effectParams.contains("shake")) shakeIntensity = ev.effectParams["shake"].get<float>();
        }
        FearEffects::StartOverlay(intensity, ev.duration, stage);
        FearEffects::StartShake(shakeIntensity, ev.duration * 0.6f);
    });

    // 背景パスを登録（遅延ロード） - AssetManager を使って安定したパスを取得
    m_bgPath = AssetManager::GetTexturePath("dark-tunnel2.jpg");
    m_bgLoadedAttempted = false;

    // Do not auto-start playback until a story is loaded by SceneManager.
    // If a story was preloaded by external code, caller may invoke Play().
}

void StoryPlayer::Update() {
    // 毎フレーム呼ばれる Update から実際の更新処理を呼ぶ
    // Use steady clock to decouple from ImGui timing
    static std::chrono::steady_clock::time_point s_lastTick = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> delta = now - s_lastTick;
    s_lastTick = now;
    float dt = delta.count();
    if (dt > 0.5f) dt = 0.5f; // clamp large dt
    UpdateImpl(dt);
}

bool StoryPlayer::LoadFromFile(const std::string& path)
{
    m_lastLoadError.clear();

    namespace fs = std::filesystem;

    try {
        fs::path ppath = Utf8ToFilesystemPath(path);
        if (!fs::exists(ppath)) {
            m_lastLoadError = "Story file does not exist: " + path;
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        std::ifstream ifs(ppath, std::ios::binary);
        if (!ifs.is_open()) {
            m_lastLoadError = "Failed to open story file: " + path;
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        json j;
        if (!json::accept(content)) {
            m_lastLoadError = "Failed to parse JSON: invalid JSON (accept failed)";
            ::Log::Log(::Log::Level::Error, m_lastLoadError + std::string(" : ") + path);
            return false;
        }

        try {
            j = json::parse(content);
        }
        catch (const std::exception& ex) {
            m_lastLoadError = std::string("Failed to parse JSON: ") + ex.what();
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        json eventsArray;
        m_characters.clear();
        if (j.is_array()) {
            eventsArray = j;
        }
        else if (j.is_object() && j.contains("events") && j["events"].is_array()) {
            eventsArray = j["events"];
            if (j.contains("characters")) {
                try {
                    ParseCharactersJson(j["characters"], m_characters);
                }
                catch (...) {
                    ::Log::Log(::Log::Level::Warning, "StoryPlayer: failed to parse characters array in story JSON");
                }
            }
        }
        else {
            m_lastLoadError = "Story JSON root is not an array or object with 'events': " + path;
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }

        m_events.clear();
        try {
            for (auto& it : eventsArray) {
                StoryEvent ev;
                if (it.contains("text")) ev.text = it.value("text", "");
                if (it.contains("speaker")) ev.speaking = it.value("speaker", "");
                if (it.contains("face")) ev.faceImage = it.value("face", "");
                if (it.contains("effect")) ev.effect = it.value("effect", "");
                if (it.contains("duration")) ev.duration = it.value("duration", 1.0f);
                if (it.contains("effectParams")) ev.effectParams = it["effectParams"];
                else ev.effectParams = nullptr;
                m_events.push_back(ev);
            }
        }
        catch (const std::exception& ex) {
            m_lastLoadError = std::string("Error reading story entries: ") + ex.what();
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            m_events.clear();
            return false;
        }

        m_index = 0;
        m_timer = 0.0f;
        m_playing = false;
        m_lastLoadError.clear();
        return true;
    }
    catch (const std::exception& ex) {
        m_lastLoadError = std::string("Filesystem/IO error: ") + ex.what();
        ::Log::Log(::Log::Level::Error, m_lastLoadError);
        return false;
    }
}

bool StoryPlayer::SaveToFile(const std::string& path) const
{
    try {
        json j = json::array();
        for (const auto& ev : m_events) {
            json it;
            it["text"] = ev.text;
            it["speaker"] = ev.speaking;
            it["face"] = ev.faceImage;
            it["effect"] = ev.effect;
            it["duration"] = ev.duration;
            if (!ev.effectParams.is_null()) it["effectParams"] = ev.effectParams;
            j.push_back(it);
        }

        std::ofstream ofs(path);
        if (!ofs.is_open()) return false;
        ofs << j.dump(2);
        return true;
    }
    catch (...) {
        return false;
    }
}

void StoryPlayer::RegisterEffect(const std::string& name, EffectHandler handler) {
    m_effects[name] = handler;
}

void StoryPlayer::TriggerEffect(const StoryEvent& ev) {
    auto it = m_effects.find(ev.effect);
    if (it != m_effects.end()) {
        it->second(ev);
    } else {
        ::Log::Log(::Log::Level::Warning, std::string("[Effect] ") + ev.effect + " (no handler)");
    }
}

void StoryPlayer::Play() {
    if (m_events.empty()) return;
    m_playing = true;
    m_timer = 0.0f;
    TriggerEffect(m_events[m_index]);
    ShowCurrentText();
}

void StoryPlayer::Pause() { m_playing = false; }

void StoryPlayer::Next() {
    if (m_events.empty()) return;
    if (onEventFinished) {
        onEventFinished(m_events[m_index]);
        if (!m_playing) return;
    }
    m_index++;
    m_timer = 0.0f;
    if (m_index >= (int)m_events.size()) { m_playing = false; return; }
    TriggerEffect(m_events[m_index]);
    ShowCurrentText();
}

void StoryPlayer::Reset() {
    m_index = 0;
    m_timer = 0.0f;
    m_playing = false;
}

void StoryPlayer::StartFadeToBattle(float duration) {
    if (m_fsFadingOut) return;
    m_fsFadingOut = true;
    m_fsFadeElapsed = 0.0f;
    m_fsFadeDuration = duration;
    m_fsFadeAlpha = 0.0f;
}

void StoryPlayer::UpdateImpl(float dt) {
    // --- 遅延ロードをここで試す ---
    LoadBackgroundTextureIfNeeded();

    // Editor preview handling: advance preview timer and restore state when done
    if (m_previewActive) {
        // Make sure effects advance during preview
        FearEffects::Update(dt);
        m_previewElapsed += dt;
        if (m_previewElapsed >= m_previewDuration) {
            // end preview and restore state
            StopPreview();
        }
        return; // while previewing, skip normal progression
    }

    // If using node graph, we don't use the linear m_events progression here.
    if (m_usingNodeGraph) {
        // Ensure effects advance
        FearEffects::Update(dt);
        return;
    }

    if (!m_playing || m_events.empty() || m_index >= (int)m_events.size()) {
        if (m_fsFadingOut) {
            m_fsFadeElapsed += dt;
            float r = (m_fsFadeDuration > 0.0f) ? (m_fsFadeElapsed / m_fsFadeDuration) : 1.0f;
            m_fsFadeAlpha = std::max(0.0f, std::min(1.0f, r));
            if (r >= 1.0f) {
                m_fsFadingOut = false;
                m_fsFadeAlpha = 1.0f;
                g_SceneManager.ChangeScene(SceneType::BATTLE);
            }
        }
        return;
    }

    if (m_fsFadingOut) {
        m_fsFadeElapsed += dt;
        float r = (m_fsFadeDuration > 0.0f) ? (m_fsFadeElapsed / m_fsFadeDuration) : 1.0f;
        m_fsFadeAlpha = std::max(0.0f, std::min(1.0f, r));
        if (r >= 1.0f) {
            m_fsFadingOut = false;
            m_fsFadeAlpha = 1.0f;
            g_SceneManager.ChangeScene(SceneType::BATTLE);
            return;
        }
        // フェード中はイベント進行を止める（任意）
        return;
    }

    // FearEffects の時間を進める（オーバーレイや揺れの内部タイマー）
    FearEffects::Update(dt);

    m_timer += dt;
    float dur = m_events[m_index].duration;
    if (m_timer >= dur) {
        const StoryEvent& ev = m_events[m_index];
        // battle_start は即時遷移ではなくフェード開始に置き換え
        if (ev.effect == "battle_start")
        {
            StartFadeToBattle(0.8f);
            return;
        }
        // イベント完了
        if (onEventFinished) onEventFinished(m_events[m_index]);
        m_index++;
        m_timer = 0.0f;
        if (m_index < (int)m_events.size()) {
            TriggerEffect(m_events[m_index]);
            ShowCurrentText();
        } else m_playing = false;
    }
}

void StoryPlayer::Render() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    // If using node graph mode, render node UI instead of linear events
    if (m_usingNodeGraph) {
        UpdateNode();
        return;
    }

    if (m_index >= (int)m_events.size()) return;
    const StoryEvent& ev = m_events[m_index];


    // --- デバッグオーバーレイ: 背景読み込みステータスを画面左上に描画 (Dev モード限定) ---
    if (g_SceneManager.IsDevMode()) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        if (fg && vp) {
            std::string existsStr = "(n/a)";
            if (!m_bgPath.empty()) {
                existsStr = (std::filesystem::exists(m_bgPath) ? "yes" : "no");
                // 代替パスも確認（出力用メッセージ）
                std::string alt = std::filesystem::current_path().string() + "/" + m_bgPath;
                if (existsStr == "no" && std::filesystem::exists(alt)) existsStr = "yes(alt)";
            }

            std::string s;
            s += "BG path: " + (m_bgPath.empty() ? std::string("(empty)") : m_bgPath) + "\n";
            s += "exists: " + existsStr + "\n";
            s += "loadAttempted: " + std::string(m_bgLoadedAttempted ? "yes" : "no") + "\n";
            s += "bgTex set: " + std::string(m_bgTex ? "yes" : "no") + "\n";
            if (!m_lastLoadError.empty()) {
                s += "Last load error: ";
                s += m_lastLoadError + "\n";
            }

            ImU32 col = ImGui::GetColorU32(ImVec4(1.0f, 0.9f, 0.2f, 1.0f));
            ImFont* font = ImGui::GetFont();
            float fontSize = ImGui::GetFontSize();
            fg->AddText(font, fontSize, ImVec2(vp->Pos.x + 8.0f, vp->Pos.y + 8.0f), col, s.c_str());
        }
    }

    // --- 背景描画（あれば） ---
    if (g_SceneManager.IsDevMode() || m_bgTex) {
        if (vp) {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            if (m_bgTex) {
                bg->AddImage(m_bgTex, vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
            }
            else {
                // Dev モードでは背景が無いことを薄いグレーで示す
                ImU32 col = ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
                bg->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
            }
            // draw characters for linear mode as well
            DrawCharacters(m_characters, bg, vp);
        }
    }

    ImVec2 shake = FearEffects::GetShakeOffset();
    FearEffects::RenderOverlay();

    ImVec2 basePos(10.0f, 600.0f);
    ImVec2 posWithShake(basePos.x + shake.x, basePos.y + shake.y);

    ImGui::SetNextWindowPos(posWithShake, ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_dialogSize, ImGuiCond_Always);

    ImFont* font = ImGui::GetFont();
    float prevScale = 1.0f;
    if (font) { prevScale = font->Scale; font->Scale = m_textScale; }

    ImGui::Begin("Dialog", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    if (!ev.speaking.empty()) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.6f, 1), "%s", ev.speaking.c_str());
        ImGui::Spacing();
    }

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + m_dialogSize.x - 16.0f);
    ImGui::TextWrapped("%s", ev.text.c_str());
    ImGui::PopTextWrapPos();

    ImGui::Separator();
    ImGui::Text("Effect: %s  Time: %.2f/%.2f", ev.effect.c_str(), m_timer, ev.duration);
    if (ImGui::Button("Play")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Pause")) Pause();
    ImGui::SameLine();
    if (ImGui::Button("Next")) Next();
    ImGui::End();

    if (font) font->Scale = prevScale;

    if ((m_fsFadingOut || m_fsFadeAlpha > 0.0f) && vp) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        if (fg) {
            ImU32 col = ImGui::GetColorU32(ImVec4(0, 0, 0, m_fsFadeAlpha));
            fg->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
        }
    }
}

void StoryPlayer::ShowCurrentText() {
    // Update character visibility/expression based on current node or event
    if (m_usingNodeGraph) {
        auto it = m_nodeMap.find(m_currentNodeId);
        if (it == m_nodeMap.end()) return;
        EventNode &node = it->second;
        std::string sp = node.speaker;
        std::string expr = node.speakerExpression;
        if (!sp.empty()) {
            for (auto &c : m_characters) {
                if (c.id == sp) {
                    c.visible = true;
                    if (!expr.empty()) c.expression = expr;
                } else {
                    c.visible = false;
                }
            }
            this->LoadCharacterTexturesIfNeeded();
        }
    } else {
        if (m_index < (int)m_events.size()) {
            const StoryEvent &ev = m_events[m_index];
            std::string sp = ev.speaking;
            std::string face = ev.faceImage;
            if (!sp.empty()) {
                for (auto &c : m_characters) {
                    if (c.id == sp) {
                        c.visible = true;
                        if (!face.empty()) c.expression = face;
                    } else {
                        c.visible = false;
                    }
                }
                this->LoadCharacterTexturesIfNeeded();
            }
        }
    }
}