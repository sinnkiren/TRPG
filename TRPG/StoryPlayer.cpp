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
    onEventFinished = [](const StoryEvent& ev) {
        if (ev.effect == "battle_start") {
            g_SceneManager.ChangeScene(SceneType::BATTLE);
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

void StoryPlayer::UpdateImpl(float dt) {
    if (!m_playing || m_events.empty() || m_index >= (int)m_events.size()) return;

    // FearEffects の時間を進める（オーバーレイや揺れの内部タイマー）
    FearEffects::Update(dt);

    m_timer += dt;
    float dur = m_events[m_index].duration;
    if (m_timer >= dur) {
        const StoryEvent& ev = m_events[m_index];
        if (ev.effect == "battle_start")
        {
            g_SceneManager.ChangeScene(SceneType::BATTLE);
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

    // FearEffects のオフセット（揺れ）を取得してダイアログ位置に反映する
    ImVec2 shake = FearEffects::GetShakeOffset();
    ImVec2 basePos(10.0f, 600.0f);
    ImVec2 posWithShake(basePos.x + shake.x, basePos.y + shake.y);

    // オーバーレイを描画（前景に描画するので Begin の前後どちらでも可）
    FearEffects::RenderOverlay();

    // ダイアログのサイズと文字スケールを適用
    ImGui::SetNextWindowPos(posWithShake, ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_dialogSize, ImGuiCond_Always);

    // 文字スケール適用 (簡易手法)
    ImFont* font = ImGui::GetFont();
    float prevScale = 1.0f;
    if (font) { prevScale = font->Scale; font->Scale = m_textScale; }

    ImGui::Begin("Dialog", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    if (!ev.speaking.empty()) ImGui::TextColored(ImVec4(1, 0.8f, 0.6f, 1), "%s", ev.speaking.c_str());
    ImGui::TextWrapped("%s", ev.text.c_str());
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
}

void StoryPlayer::ShowCurrentText() {
    // テキストや顔切替の仕込みをここに（キャッシュや描画準備）
    // 例: LoadFaceTexture(m_events[m_index].faceImage);
}
