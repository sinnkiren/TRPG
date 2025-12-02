#include "StoryPlayer.h"
#include "SceneManager.h"
#include <fstream>
#include <iostream>
#include "system/imgui/imgui.h"
#include "system/json.hpp" 
#include "FearEffects.h"

using json = nlohmann::json;

StoryPlayer::StoryPlayer() {}
StoryPlayer::~StoryPlayer() {}

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
        FearEffects::StartShake(shakeIntensity, ev.duration * 0.6f); // 血は少し速めに収束させる等の調整
        });

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
        // effectParams があれば読み込む（無ければ null を保持）
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
    // 登録されていれば呼び出す
    auto it = m_effects.find(ev.effect);
    if (it != m_effects.end()) {
        it->second(ev);
    }
    else {
        // 無ければデフォルトの振る舞い（ログ）
        std::cout << "[Effect] " << ev.effect << " (no handler)\n";
    }
}

void StoryPlayer::Play() {
    if (m_events.empty()) return;
    m_playing = true;
    m_timer = 0.0f;
    // 初回イベントの効果をすぐ発火する
    TriggerEffect(m_events[m_index]);
    ShowCurrentText();
}

void StoryPlayer::Pause() {
    m_playing = false;
}

void StoryPlayer::Next() {
    if (m_events.empty()) return;

    // イベント完了コールバック
    if (onEventFinished) {
        onEventFinished(m_events[m_index]);

        // もしシーンが変わった場合、this は破棄されるので以降の処理をやめる
        if (!m_playing) return;
    }

    m_index++;
    m_timer = 0.0f;
    if (m_index >= (int)m_events.size()) {
        m_playing = false;
        return;
    }

    // 次イベントを発火
    TriggerEffect(m_events[m_index]);
    ShowCurrentText();
}

void StoryPlayer::Reset() {
    m_index = 0;
    m_timer = 0.0f;
    m_playing = false;
}

void StoryPlayer::StartFadeToBattle(float duration)
{
    if (m_fsFadingOut) return; // 既にフェード中なら何もしない
    m_fsFadingOut = true;
    m_fsFadeElapsed = 0.0f;
    m_fsFadeDuration = duration;
    m_fsFadeAlpha = 0.0f;
}

void StoryPlayer::UpdateImpl(float dt) {
    if (!m_playing || m_events.empty() || m_index >= (int)m_events.size()) {
        // ただしフェードが動いている場合は継続処理する
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

    // フェード進行があるなら優先して進める
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
        }
        else {
            m_playing = false;
        }
    }

    // --- ImGui デバッグUI（オプション） ---
    // ここを呼び出し元のUIコードで描画しても良い
}

void StoryPlayer::Render() {
    if (m_index >= (int)m_events.size()) return;
    const StoryEvent& ev = m_events[m_index];

    // --- FearEffects の揺れオフセットを取得 ---
    ImVec2 shake = FearEffects::GetShakeOffset();

    // --- オーバーレイを描画（前景に描画するので Begin の前後どちらでも可） ---
    FearEffects::RenderOverlay();

    // 元の位置（左下寄せ）に戻す: basePos(10,600) に揺れを加える
    ImVec2 basePos(10.0f, 600.0f);
    ImVec2 posWithShake(basePos.x + shake.x, basePos.y + shake.y);

    ImGui::SetNextWindowPos(posWithShake, ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_dialogSize, ImGuiCond_Always);

    // 文字スケール適用 (簡易手法)
    ImFont* font = ImGui::GetFont();
    float prevScale = 1.0f;
    if (font) { prevScale = font->Scale; font->Scale = m_textScale; }

    ImGui::Begin("Dialog", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    // スピーカー名（左寄せ）
    if (!ev.speaking.empty()) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.6f, 1), "%s", ev.speaking.c_str());
        ImGui::Spacing();
    }

    // テキストは ImGui のラップで描画（左揃え、元の位置）
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

    // スケールを元に戻す
    if (font) font->Scale = prevScale;

    // フェード中は画面全体を覆う黒矩形を描画（UI の上）
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if ((m_fsFadingOut || m_fsFadeAlpha > 0.0f) && vp) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        if (fg) {
            ImU32 col = ImGui::GetColorU32(ImVec4(0, 0, 0, m_fsFadeAlpha));
            fg->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
        }
    }
}

void StoryPlayer::ShowCurrentText() {
    // テキストや顔切替の仕込みをここに（キャッシュや描画準備）
    // 例: LoadFaceTexture(m_events[m_index].faceImage);
}