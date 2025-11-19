#include "FearEffects.h"
#include "system/imgui/imgui.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <array>
#include <cstdint>

namespace FearEffects
{
    // ---- 状態 ----
    float g_shakeIntensity = 0.0f;
    float g_shakeDuration = 0.0f;
    float g_shakeElapsed = 0.0f;
    float g_shakePhaseX = 0.0f;
    float g_shakePhaseY = 0.0f;

    float g_overlayIntensity = 0.0f;
    float g_overlayDuration = 0.0f;
    float g_overlayElapsed = 0.0f;
    int   g_overlayStage = 0;

    // ---- ランタイム調整可能なノイズパラメータ（濃く見える初期値） ----
    // これらは SetOverlayNoise... で実行時に変更できます
    static float s_noiseScale = 0.0025f;      // 空間スケール（小さいほど大きなうねり＝粗め）
    static int   s_noiseBaseCount = 700;      // ベースのノイズ数（密度） ← 大きくして濃くする
    static float s_noiseAlphaScale = 2.5f;    // アルファ倍率（1.0 = 既定） ← 大きめで濃く
    static float s_noiseMinSize = 1.0f;       // ノイズ矩形の最小サイズ（px）
    static float s_noiseMaxSize = 6.0f;       // ノイズ矩形の最大サイズ（px） ← 大きめに
    static float s_noiseTimeSpeed = 0.9f;     // 時間進行速度（1.0 = 既定） ← 少し速め
    static float s_noiseContrastBase = 0.9f;  // コントラスト基準（1.0が標準）

    // 乱数
    std::mt19937& GetRng()
    {
        static std::random_device rd;
        static std::mt19937 rng(rd());
        return rng;
    }

    // ---- パーミュテーションテーブル ----
    std::array<uint8_t, 256> s_perm;
    void FE_InitNoisePerm()
    {
        for (int i = 0; i < 256; ++i) s_perm[i] = static_cast<uint8_t>(i);
        std::shuffle(s_perm.begin(), s_perm.end(), GetRng());
    }

    inline float FE_Smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

    float FE_Noise1D(float x)
    {
        static bool s_init = (FE_InitNoisePerm(), true);
        int xi = static_cast<int>(std::floor(x));
        float xf = x - static_cast<float>(xi);
        uint8_t i0 = s_perm[static_cast<uint8_t>(xi & 0xFF)];
        uint8_t i1 = s_perm[static_cast<uint8_t>((xi + 1) & 0xFF)];
        float v0 = (static_cast<int>(i0) / 255.0f) * 2.0f - 1.0f;
        float v1 = (static_cast<int>(i1) / 255.0f) * 2.0f - 1.0f;
        float u = FE_Smoothstep(xf);
        return v0 * (1.0f - u) + v1 * u;
    }

    float FE_FractalNoise1D(float x, int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
    {
        float sum = 0.0f, amp = 1.0f, freq = 1.0f, maxAmp = 0.0f;
        for (int i = 0; i < octaves; ++i) { sum += FE_Noise1D(x * freq) * amp; maxAmp += amp; amp *= gain; freq *= lacunarity; }
        return sum / maxAmp;
    }

    // ---- Perlin 2D 実装 ----
    float FE_Perlin2D(float x, float y)
    {
        static bool s_init = (FE_InitNoisePerm(), true);
        int x0 = static_cast<int>(std::floor(x));
        int y0 = static_cast<int>(std::floor(y));
        float xf = x - static_cast<float>(x0);
        float yf = y - static_cast<float>(y0);
        float u = FE_Smoothstep(xf), v = FE_Smoothstep(yf);
        auto hash = [&](int xi, int yi) -> int {
            return s_perm[(s_perm[static_cast<uint8_t>(xi & 0xFF)] + static_cast<uint8_t>(yi & 0xFF)) & 0xFF];
            };
        int h00 = hash(x0, y0), h10 = hash(x0 + 1, y0), h01 = hash(x0, y0 + 1), h11 = hash(x0 + 1, y0 + 1);
        static const float G[8][2] = {
            { 1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f,-1.0f},
            { 0.70710678f, 0.70710678f}, {-0.70710678f, 0.70710678f}, {0.70710678f,-0.70710678f}, {-0.70710678f,-0.70710678f}
        };
        auto gradDot = [&](int h, float dx, float dy) -> float { int gi = h & 7; return G[gi][0] * dx + G[gi][1] * dy; };
        float d00 = gradDot(h00, xf, yf);
        float d10 = gradDot(h10, xf - 1.0f, yf);
        float d01 = gradDot(h01, xf, yf - 1.0f);
        float d11 = gradDot(h11, xf - 1.0f, yf - 1.0f);
        float ix0 = d00 + u * (d10 - d00);
        float ix1 = d01 + u * (d11 - d01);
        return ix0 + v * (ix1 - ix0);
    }

    float FE_FractalNoise2D(float x, float y, int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
    {
        float sum = 0.0f, amp = 1.0f, freq = 1.0f, maxAmp = 0.0f;
        for (int i = 0; i < octaves; ++i) { sum += FE_Perlin2D(x * freq, y * freq) * amp; maxAmp += amp; amp *= gain; freq *= lacunarity; }
        return sum / maxAmp;
    }

    // ---- セッター実装 ----
    void SetOverlayNoiseScale(float scale) { s_noiseScale = std::max(1e-6f, scale); }
    void SetOverlayNoiseBaseCount(int count) { s_noiseBaseCount = std::clamp(count, 1, 20000); }
    void SetOverlayNoiseAlphaScale(float a) { s_noiseAlphaScale = std::max(0.0f, a); }
    void SetOverlayNoiseSizeRange(float minS, float maxS) {
        s_noiseMinSize = std::max(0.0f, minS);
        s_noiseMaxSize = std::max(s_noiseMinSize, maxS);
    }
    void SetOverlayNoiseTimeSpeed(float speed) { s_noiseTimeSpeed = std::max(0.0f, speed); }
    void SetOverlayNoiseContrastBase(float c) { s_noiseContrastBase = std::max(0.01f, c); }

    // ---- 既存の API ----
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
        float clampedIntensity = std::clamp(intensity, 0.0f, 1.0f);
        int clampedStage = std::clamp(stage, 0, 4);
        float clampedDuration = std::max(0.0f, duration);
        float remaining = std::max(0.0f, g_overlayDuration - g_overlayElapsed);
        g_overlayIntensity = std::max(g_overlayIntensity, clampedIntensity);
        g_overlayDuration = std::max(remaining, clampedDuration);
        g_overlayElapsed = 0.0f;
        g_overlayStage = std::max(g_overlayStage, clampedStage);
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

    // ---- RenderOverlay（ランタイムパラメータを使用） ----
    void RenderOverlay()
    {
        if (g_overlayDuration <= 0.0f || g_overlayElapsed >= g_overlayDuration) return;

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImDrawList* dl = ImGui::GetForegroundDrawList();

        float t = 1.0f - (g_overlayElapsed / g_overlayDuration);
        float intensity = std::clamp(g_overlayIntensity, 0.0f, 1.0f) * t;
        int stage = std::clamp(g_overlayStage, 0, 4);
        float stageBoost = 1.0f + 0.35f * static_cast<float>(stage);

        // パラメータ（ランタイムで変更可能な s_* を利用）
        int baseCount = s_noiseBaseCount;
        int noiseCount = static_cast<int>(baseCount * stageBoost * (0.7f + intensity));
        noiseCount = std::clamp(noiseCount, 8, 20000);

        float minSize = s_noiseMinSize;
        float maxSize = s_noiseMaxSize + stage * 2.0f; // stage で大きさを増す

        // ドメインワープ設定（荒さを作る核）
        float warpScale = std::max(1e-5f, s_noiseScale * 6.0f);   // ワープの周波数
        float warpStrength = 0.6f + 1.2f * intensity * (0.5f + 0.5f * stageBoost); // ワープの強さ

        float noiseScale = s_noiseScale * (1.0f + 0.25f * stage);
        float timeBase = g_overlayElapsed * s_noiseTimeSpeed;

        // 斑点生成のしきい値（強度が高いほどしきい値下がり、斑点が増える）
        float blotchThreshold = std::clamp(0.72f - 0.35f * intensity, 0.25f, 0.85f);

        // 格子サンプルで均等に配置（効率的）
        int cols = static_cast<int>(std::sqrt(static_cast<float>(noiseCount)));
        if (cols < 1) cols = 1;
        int rows = (noiseCount + cols - 1) / cols;

        int idx = 0;
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                if (idx >= noiseCount) break;
                float fx = (c + 0.5f) / static_cast<float>(cols);
                float fy = (r + 0.5f) / static_cast<float>(rows);

                // ワールド座標ベースのノイズ入力
                float sx = (vp->Pos.x + fx * vp->Size.x) * noiseScale;
                float sy = (vp->Pos.y + fy * vp->Size.y) * noiseScale;

                // ドメインワープ量（フラクタルノイズで滑らかに変化させる）
                float wx = FE_FractalNoise2D(sx * warpScale + timeBase * 0.18f, sy * warpScale - timeBase * 0.12f, 3) * warpStrength;
                float wy = FE_FractalNoise2D(sx * warpScale - timeBase * 0.11f, sy * warpScale + timeBase * 0.14f, 3) * warpStrength;

                // ワープ後のサンプル
                float nx = sx + wx;
                float ny = sy + wy;

                float v = FE_FractalNoise2D(nx + timeBase * 0.25f, ny - timeBase * 0.21f, 4, 2.0f, 0.5f);
                float gv = 0.5f * (v + 1.0f); // 0..1

                // コントラスト調整
                float contrast = s_noiseContrastBase + 1.5f * intensity;
                float gvC = std::pow(gv, 1.0f / contrast);

                // 斑点化: しきい値以上なら大きめのブロッチ、未満は小さな粒
                bool isBlotch = (gvC > blotchThreshold);

                float alphaBase = std::clamp(0.08f * intensity * (0.5f + 0.5f * stageBoost) * s_noiseAlphaScale, 0.001f, 0.9f);
                float alpha = isBlotch ? (alphaBase * (0.9f + 1.2f * (gvC - blotchThreshold))) : (alphaBase * 0.35f * gvC);

                // 大きさ：ブロッチは大きめ
                float size = isBlotch ? (maxSize * (0.6f + 0.8f * (gvC - blotchThreshold))) : (minSize + std::fmod(static_cast<float>(idx) * 7.13f + fy * 3.71f, 1.0f) * (maxSize - minSize) * 0.6f);
                if (size < 0.5f) size = 0.5f;

                float px = vp->Pos.x + fx * vp->Size.x;
                float py = vp->Pos.y + fy * vp->Size.y;

                // 若干の暗化で恐怖感（グレースケール）
                float gray = isBlotch ? std::clamp(0.05f + 0.85f * (1.0f - gvC), 0.0f, 1.0f) : std::clamp(0.2f + 0.8f * gvC, 0.0f, 1.0f);

                ImU32 col = ImGui::GetColorU32(ImVec4(gray, gray, gray, alpha));
                dl->AddRectFilled(ImVec2(px, py), ImVec2(px + size, py + size), col);

                ++idx;
            }
        }

        // 横スキャンラインを少し強めにして荒さを補強
        int lines = 4 + stage * 2;
        for (int i = 0; i < lines; ++i)
        {
            float y = vp->Pos.y + ((i + 1) / static_cast<float>(lines + 1)) * vp->Size.y;
            float ln = FE_Perlin2D(y * noiseScale * 0.6f + timeBase * 0.2f, i * 0.37f);
            float la = std::clamp(0.02f * intensity * (0.9f + 0.6f * ln), 0.0f, 0.18f) * s_noiseAlphaScale;
            ImU32 col = ImGui::GetColorU32(ImVec4(0.45f, 0.45f, 0.45f, la));
            dl->AddRectFilled(ImVec2(vp->Pos.x, y), ImVec2(vp->Pos.x + vp->Size.x, y + 1.0f), col);
        }
    }
}