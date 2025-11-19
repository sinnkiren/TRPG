#include "FearEffects.h"
#include "system/imgui/imgui.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <array>
#include <cstdint>

namespace
{
    // シェイク関連の状態
    float g_shakeIntensity = 0.0f;
    float g_shakeDuration = 0.0f;
    float g_shakeElapsed = 0.0f;
    float g_shakePhaseX = 0.0f;
    float g_shakePhaseY = 0.0f;

    // オーバーレイ関連の状態
    float g_overlayIntensity = 0.0f;
    float g_overlayDuration = 0.0f;
    float g_overlayElapsed = 0.0f;
    int   g_overlayStage = 0;

    // 乱数ジェネレータ（ユーティリティ）
    std::mt19937& GetRng()
    {
        static std::random_device rd;
        static std::mt19937 rng(rd());
        return rng;
    }

    // ---- 簡易 1D ノイズ実装（名前衝突回避のためプレフィックスを付与） ----
    std::array<uint8_t, 256> s_perm;

    void FE_InitNoisePerm()
    {
        for (int i = 0; i < 256; ++i) s_perm[i] = static_cast<uint8_t>(i);
        std::shuffle(s_perm.begin(), s_perm.end(), GetRng());
    }

    inline float FE_Smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

    float FE_Noise1D(float x)
    {
        // permutation が初期化済みであることを保証
        static bool s_init = (FE_InitNoisePerm(), true);

        int xi = static_cast<int>(std::floor(x));
        float xf = x - static_cast<float>(xi);

        uint8_t i0 = s_perm[static_cast<uint8_t>(xi & 0xFF)];
        uint8_t i1 = s_perm[static_cast<uint8_t>((xi + 1) & 0xFF)];

        // 0..255 -> -1..1 にマップ
        float v0 = (static_cast<int>(i0) / 255.0f) * 2.0f - 1.0f;
        float v1 = (static_cast<int>(i1) / 255.0f) * 2.0f - 1.0f;

        float u = FE_Smoothstep(xf);
        return v0 * (1.0f - u) + v1 * u;
    }

    float FE_FractalNoise1D(float x, int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
    {
        float sum = 0.0f;
        float amp = 1.0f;
        float freq = 1.0f;
        float maxAmp = 0.0f;
        for (int i = 0; i < octaves; ++i)
        {
            sum += FE_Noise1D(x * freq) * amp;
            maxAmp += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return sum / maxAmp;
    }
}

namespace FearEffects
{
    void StartShake(float intensity, float duration)
    {
        g_shakeIntensity = std::max(0.0f, intensity);
        g_shakeDuration = std::max(0.0f, duration);
        g_shakeElapsed = 0.0f;

        std::uniform_real_distribution<float> dist(0.0f, 3.14159265f * 2.0f);
        g_shakePhaseX = dist(GetRng());
        g_shakePhaseY = dist(GetRng());
    }

    void StartOverlay(float intensity, float duration, int stage)
    {
        g_overlayIntensity = std::max(0.0f, intensity);
        g_overlayDuration = std::max(0.0f, duration);
        g_overlayElapsed = 0.0f;
        g_overlayStage = stage;
    }

    void Update(float dt)
    {
        if (g_shakeElapsed < g_shakeDuration) g_shakeElapsed = std::min(g_shakeElapsed + dt, g_shakeDuration);
        if (g_overlayElapsed < g_overlayDuration) g_overlayElapsed = std::min(g_overlayElapsed + dt, g_overlayDuration);
    }

    ImVec2 GetShakeOffset()
    {
        if (g_shakeDuration <= 0.0f || g_shakeElapsed >= g_shakeDuration) return ImVec2(0.0f, 0.0f);

        float rem = 1.0f - (g_shakeElapsed / g_shakeDuration);
        float ease = 1.0f - (1.0f - rem) * (1.0f - rem);
        float mag = g_shakeIntensity * ease;

        float sinX = std::sin(g_shakeElapsed * 23.0f + g_shakePhaseX);
        float sinY = std::cos(g_shakeElapsed * 19.0f + g_shakePhaseY);

        float noiseX = FE_FractalNoise1D(g_shakeElapsed * 1.4f + g_shakePhaseX, 4);
        float noiseY = FE_FractalNoise1D(g_shakeElapsed * 1.1f + g_shakePhaseY, 4);

        const float sinWeight = 0.6f;
        const float noiseWeight = 0.4f;

        float x = mag * (sinWeight * sinX + noiseWeight * noiseX);
        float y = mag * (sinWeight * sinY + noiseWeight * noiseY);

        return ImVec2(x, y);
    }

    void RenderOverlay()
    {
        if (g_overlayDuration <= 0.0f || g_overlayElapsed >= g_overlayDuration) return;
        float alpha = g_overlayIntensity * (1.0f - (g_overlayElapsed / g_overlayDuration));
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImU32 col = ImGui::GetColorU32(ImVec4(0.6f, 0.0f, 0.0f, alpha));
        dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
    }
}