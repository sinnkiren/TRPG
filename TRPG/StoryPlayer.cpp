#include "Application.h"
#include "system/stb_image.h"
#include "StoryPlayer.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Logging.h"
#include <fstream>
#include "system/imgui/imgui.h"
#include "system/json.hpp" 
#include "FearEffects.h"
#include <filesystem>
#include <d3d11.h>
#include <direct.h> // _getcwd

using json = nlohmann::json;

StoryPlayer::StoryPlayer() {}
StoryPlayer::~StoryPlayer() {
#ifdef IMGUI_IMPL_DIRECTX11
    if (m_bgSrv) { m_bgSrv->Release(); m_bgSrv = nullptr; }
#endif
}

void StoryPlayer::RenderDevPanelContents()
{
    ImGui::Text("StoryPlayer Dev Info");
    ImGui::Separator();
    ImGui::Text("BG Path: %s", m_bgPath.empty() ? "(empty)" : m_bgPath.c_str());
    ImGui::Text("Load attempts: %d", m_bgLoadAttempts);
    ImGui::Text("Loaded attempted: %s", m_bgLoadedAttempted ? "yes" : "no");
    if (!m_lastLoadError.empty()) ImGui::TextWrapped("Last load error: %s", m_lastLoadError.c_str());
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
}

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
    LoadFromFile("assets/story/story.json");

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

    // 背景パスを登録（遅延ロード） - TextureManager の assetRoot を "assets/texture/" にしているため相対パスで指定
    m_bgPath = "texture/dark-tunnel2.jpg";
    m_bgLoadedAttempted = false;

    Play();
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

bool StoryPlayer::LoadFromFile(const std::string& path) {
    // Clear previous error
    m_lastLoadError.clear();

    // Check file existence first
    namespace fs = std::filesystem;
    try {
        if (!fs::exists(path)) {
            m_lastLoadError = "Story file does not exist: " + path;
            ::Log::Log(::Log::Level::Error, m_lastLoadError);
            return false;
        }
    }
    catch (const std::exception& ex) {
        m_lastLoadError = std::string("Filesystem check failed: ") + ex.what();
        ::Log::Log(::Log::Level::Error, m_lastLoadError);
        return false;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        m_lastLoadError = "Failed to open story file: " + path;
        ::Log::Log(::Log::Level::Error, m_lastLoadError);
        return false;
    }

    json j;
    try {
        ifs >> j;
    }
    catch (const std::exception& ex) {
        m_lastLoadError = std::string("Failed to parse JSON: ") + ex.what();
        ::Log::Log(::Log::Level::Error, m_lastLoadError);
        return false;
    }

    // Validate that the root is an array
    if (!j.is_array()) {
        m_lastLoadError = "Story JSON root is not an array: " + path;
        ::Log::Log(::Log::Level::Error, m_lastLoadError);
        return false;
    }

    m_events.clear();
    try {
        for (auto& it : j) {
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
    // success: clear last error
    m_lastLoadError.clear();
    return true;
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
    if (m_index >= (int)m_events.size()) return;
    const StoryEvent& ev = m_events[m_index];

    const ImGuiViewport* vp = ImGui::GetMainViewport();

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
    // 例: LoadFaceTexture(m_events[m_index].faceImage);
}