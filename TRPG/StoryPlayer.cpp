#include "Application.h"
#include "system/stb_image.h"
#include "StoryPlayer.h"
#include "SceneManager.h"
#include <fstream>
#include <iostream>
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

#ifdef IMGUI_IMPL_DIRECTX11
void StoryPlayer::SetBackgroundSRV(ID3D11ShaderResourceView* srv)
{
    if (m_bgSrv == srv) return;
    if (m_bgSrv) { m_bgSrv->Release(); m_bgSrv = nullptr; }
    m_bgSrv = srv;
    m_bgTex = reinterpret_cast<ImTextureID>(srv);
}
#endif

// --- 追加: 遅延ロード用ヘルパー ---
// m_bgPath にパスをセットしておけば、Device が準備できた時点でここでロードします。

void StoryPlayer::LoadBackgroundTextureIfNeeded()
{
    if (m_bgLoadedAttempted) return; // 既に試行済み
    if (m_bgPath.empty()) return;

#ifdef IMGUI_IMPL_DIRECTX11
    // Device が準備できているかチェック
    if (!Application::GetDevice()) {
        // Device 未初期化 => 後で試す
        return;
    }

    m_bgLoadedAttempted = true; // 一度だけ試す
    const std::string path = m_bgPath;
    // 実行時カレントディレクトリをログ出力
    char cwdBuf[1024] = { 0 };
    if (_getcwd(cwdBuf, sizeof(cwdBuf))) {
        std::string cwd(cwdBuf);
        std::cerr << "StoryPlayer: CWD = " << cwd << "\n";
        OutputDebugStringA(("StoryPlayer: CWD = " + cwd + "\n").c_str());
    }

    // ファイル存在確認（相対/絶対）
    bool exists = std::filesystem::exists(path);
    std::cerr << "StoryPlayer: checking path: " << path << " exists=" << (exists ? "yes" : "no") << "\n";
    OutputDebugStringA(("StoryPlayer: checking path: " + path + (exists ? " exists\n" : " not exists\n")).c_str());

    const char* bgPath = path.c_str();
    if (!exists) {
        // デバッグ用にカレントディレクトリを付けた絶対パスを試す
        std::string alt = std::filesystem::current_path().string() + "/" + path;
        if (std::filesystem::exists(alt)) {
            bgPath = alt.c_str();
            std::cerr << "StoryPlayer: found at alt path: " << alt << "\n";
            OutputDebugStringA(("StoryPlayer: found at alt path: " + alt + "\n").c_str());
        }
        else {
            std::cerr << "StoryPlayer: file not found: " << path << "\n";
            OutputDebugStringA("StoryPlayer: background file not found, aborting load\n");
            return;
        }
    }

    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(bgPath, &w, &h, &channels, 4);
    if (!pixels || w <= 0 || h <= 0) {
        std::cerr << "StoryPlayer: stbi_load failed for: " << bgPath << " (w=" << w << " h=" << h << " channels=" << channels << ")\n";
        OutputDebugStringA("StoryPlayer: stbi_load failed\n");
        if (pixels) stbi_image_free(pixels);
        return;
    }

    std::cerr << "StoryPlayer: stbi_load OK: " << bgPath << " size=" << w << "x" << h << " channels=" << channels << "\n";
    OutputDebugStringA("StoryPlayer: stbi_load succeeded\n");

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = static_cast<UINT>(w);
    desc.Height = static_cast<UINT>(h);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = pixels;
    initData.SysMemPitch = static_cast<UINT>(w * 4);

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = Application::GetDevice()->CreateTexture2D(&desc, &initData, &tex);
    if (FAILED(hr) || tex == nullptr) {
        std::cerr << "StoryPlayer: CreateTexture2D failed for: " << bgPath << " hr=0x" << std::hex << hr << std::dec << "\n";
        OutputDebugStringA("StoryPlayer: CreateTexture2D failed\n");
        if (tex) tex->Release();
        stbi_image_free(pixels);
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = Application::GetDevice()->CreateShaderResourceView(tex, &srvDesc, &srv);
    if (FAILED(hr) || srv == nullptr) {
        std::cerr << "StoryPlayer: CreateShaderResourceView failed for: " << bgPath << " hr=0x" << std::hex << hr << std::dec << "\n";
        OutputDebugStringA("StoryPlayer: CreateShaderResourceView failed\n");
        if (srv) srv->Release();
        tex->Release();
        stbi_image_free(pixels);
        return;
    }

    // 成功
    SetBackgroundSRV(srv); // StoryPlayer が srv を所有する
    tex->Release();
    stbi_image_free(pixels);

    std::cerr << "StoryPlayer: background loaded: " << bgPath << " (" << w << "x" << h << ")\n";
    OutputDebugStringA("StoryPlayer: background loaded successfully\n");
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

    // 背景パスを登録（遅延ロード）
    m_bgPath = "assets/texture/dark-tunnel2.jpg"; // 実行ディレクトリに合わせて配置してください
    m_bgLoadedAttempted = false;

    Play();
}

void StoryPlayer::Update() {
    // 毎フレーム呼ばれる Update から実際の更新処理を呼ぶ
    float dt = ImGui::GetIO().DeltaTime;
    UpdateImpl(dt);
}

bool StoryPlayer::LoadFromFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open story file: " << path << "\n";
        return false;
    }
    json j;
    ifs >> j;
    m_events.clear();
    for (auto& it : j) {
        StoryEvent ev;
        ev.text = it.value("text", "");
        ev.speaking = it.value("speaker", "");
        ev.faceImage = it.value("face", "");
        ev.effect = it.value("effect", "");
        ev.duration = it.value("duration", 1.0f);
        if (it.contains("effectParams")) ev.effectParams = it["effectParams"];
        else ev.effectParams = nullptr;
        m_events.push_back(ev);
    }
    m_index = 0;
    m_timer = 0.0f;
    m_playing = false;
    return true;
}

void StoryPlayer::RegisterEffect(const std::string& name, EffectHandler handler) {
    m_effects[name] = handler;
}

void StoryPlayer::TriggerEffect(const StoryEvent& ev) {
    auto it = m_effects.find(ev.effect);
    if (it != m_effects.end()) {
        it->second(ev);
    } else {
        std::cout << "[Effect] " << ev.effect << " (no handler)\n";
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

    // --- デバッグオーバーレイ: 背景読み込みステータスを画面左上に描画 ---
    {
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

            ImU32 col = ImGui::GetColorU32(ImVec4(1.0f, 0.9f, 0.2f, 1.0f));
            ImFont* font = ImGui::GetFont();
            float fontSize = ImGui::GetFontSize();
            fg->AddText(font, fontSize, ImVec2(vp->Pos.x + 8.0f, vp->Pos.y + 8.0f), col, s.c_str());
        }
    }

    // --- 背景描画（あれば） ---
    if (m_bgTex && vp) {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        bg->AddImage(m_bgTex, vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
    } else {
        // デバッグ用: 背景が無い場合は薄いグレーで塗ることで確認可能
        if (vp) {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            ImU32 col = ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            bg->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
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