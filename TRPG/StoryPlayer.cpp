#include "StoryPlayer.h"
#include "SceneManager.h"
#include <fstream>
#include <iostream>
#include "system/imgui/imgui.h"
#include "system/json.hpp" 

using json = nlohmann::json;

StoryPlayer::StoryPlayer() {}
StoryPlayer::~StoryPlayer() {}

void StoryPlayer::Initialize() {
    LoadFromFile("assets/story/story.json");
    onEventFinished = [](const StoryEvent& ev) {
        if (ev.effect == "battle_start") {
            g_SceneManager.ChangeScene(SceneType::BATTLE);
        }
        };
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

    m_timer += dt;
    float dur = m_events[m_index].duration;
    if (m_timer >= dur) {
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

    // --- 例: 自作描画API呼び出し ---
    // DrawTextCentered(ev.text);
    // DrawFaceImage(ev.faceImage, positionLeftBottom);
    // ※実際の描画はエンジンAPIに合わせて実装してください。
    
    // ここではImGuiで簡易表示（開発中用）
    ImGui::SetNextWindowPos(ImVec2(10, 600), ImGuiCond_Always);
    ImGui::Begin("Dialog", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
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
}

void StoryPlayer::ShowCurrentText() {
    // テキストや顔切替の仕込みをここに（キャッシュや描画準備）
    // 例: LoadFaceTexture(m_events[m_index].faceImage);
}
