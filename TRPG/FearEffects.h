#pragma once
#include "system/imgui/imgui.h"
#include <cmath>
#include <algorithm>
#include <random>

// Local clamp to avoid depending on std::clamp (toolchain differences / NOMINMAX macro issues)
template<typename T>
static inline T clamp_val(T v, T lo, T hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

// FearEffects 外部 API
namespace FearEffects
{
    // expose RNG access for deterministic testing
    std::mt19937& GetRng();
    void StartShake(float intensity, float duration);
    void StartOverlay(float intensity, float duration, int stage = 0);
    void Update(float dt);
    ImVec2 GetShakeOffset();
    void RenderOverlay();

    // --- ノイズ見た目のランタイム調整 API ---
    void SetOverlayNoiseScale(float scale);
    void SetOverlayNoiseBaseCount(int count);
    void SetOverlayNoiseAlphaScale(float alphaScale);
    void SetOverlayNoiseSizeRange(float minSize, float maxSize);
    void SetOverlayNoiseTimeSpeed(float speed);
    void SetOverlayNoiseContrastBase(float contrast);
}

// 全画面の恐怖オーバーレイ描画（補助関数）
// BattleScene 等、外部から直接呼ぶユーティリティ。インラインでヘッダに置く。
inline void DrawFearOverlay(float intensity, float timeSeconds, int stage = 0)
{
    if (intensity <= 0.001f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 disp = ImGui::GetIO().DisplaySize;

    // stage によるわずかな増強（0..4）
    float stageBoost = 0.5f + 0.3f * static_cast<float>(clamp_val(stage, 0, 4));
    float redTint = 0.2f + 0.2f * static_cast<float>(clamp_val(stage, 0, 4)); // 使わない場合は無視可

    // まずうっすら暗めの覆い（画面全体）
    ImU32 dark = ImColor(0.0f, 0.0f, 0.0f, 0.08f * intensity * stageBoost);
    dl->AddRectFilled(ImVec2(0, 0), disp, dark);

    // 複数の薄い矩形で段階的な色味のレイヤーを追加（主に雰囲気作り）
    for (int i = 0; i < 3; ++i) {
        float pad = i * 40.0f * intensity * (1.0f + 0.2f * stage);
        float a = 0.02f + 0.06f * intensity * (1.0f + 0.6f * stage) * (1.0f + 0.5f * std::sin(timeSeconds * (1.2f + i)));
        // 赤一色にしない（グレー寄りの暗色を使う）
        float r = std::fmin(1.0f, 0.2f + 0.15f * i * stageBoost);
        float g = std::fmax(0.0f, 0.05f - 0.01f * i);
        float b = std::fmax(0.0f, 0.05f - 0.01f * i);
        ImU32 col = ImColor(r, g, b, a);
        dl->AddRectFilled(ImVec2(pad, pad), ImVec2(disp.x - pad, disp.y - pad), col);
    }

    // 画面ノイズの薄い粒（白っぽく）を散らす（低コスト版）
    int noiseCount = static_cast<int>(20 + stage * 10);
    noiseCount = clamp_val(noiseCount, 8, 200);
    for (int i = 0; i < noiseCount; ++i) {
        float x = std::fmod(timeSeconds * (13.0f + i * 3.7f) + i * 97.3f, disp.x);
        float y = std::fmod(timeSeconds * (19.0f + i * 2.9f) + i * 61.7f, disp.y);
        float a = 0.005f * intensity * (1.0f + 0.4f * stage);
        a = clamp_val(a, 0.0005f, 0.06f);
        ImU32 col = ImColor(1.0f, 1.0f, 1.0f, a);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 2.0f, y + 2.0f), col);
    }
}