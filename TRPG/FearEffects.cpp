#include "FearEffects.h"
#include "system/imgui/imgui.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <array>
#include <cstdint>

// FearEffects 名前空間内に内部状態・ユーティリティを集約します。
// これにより同一名前空間の公開 API（StartShake 等）から直接アクセスできます。
namespace FearEffects
{
    // -------------------------
    // シェイク（画面揺れ）関連の状態
    // -------------------------
    // 現在有効な揺れの強度（ピクセル単位や倍率など、運用に合わせる）
    float g_shakeIntensity = 0.0f;
    // 揺れの総時間（秒）
    float g_shakeDuration = 0.0f;
    // 経過時間（秒）
    float g_shakeElapsed = 0.0f;
    // 位相（乱数で初期化して複数回の呼び出しで差異を出す）
    float g_shakePhaseX = 0.0f;
    float g_shakePhaseY = 0.0f;

    // -------------------------
    // オーバーレイ（画面赤み等）関連の状態
    // -------------------------
    float g_overlayIntensity = 0.0f; // 最大アルファ等に乗算する強度
    float g_overlayDuration = 0.0f;  // 表示時間（秒）
    float g_overlayElapsed = 0.0f;   // 経過時間（秒）
    int   g_overlayStage = 0;        // ステージ（強さ段階）を示す任意値

    // -------------------------
    // 乱数・ノイズユーティリティ
    // -------------------------
    // 共有の乱数生成器を返す（内部静的保持）
    std::mt19937& GetRng()
    {
        static std::random_device rd;
        static std::mt19937 rng(rd()); // シードは環境に依存
        return rng;
    }

    // 1D ノイズ（簡易実装）用のパーミュテーションテーブル
    std::array<uint8_t, 256> s_perm;

    // パーミュテーションテーブルを初期化してシャッフルする
    void FE_InitNoisePerm()
    {
        for (int i = 0; i < 256; ++i) s_perm[i] = static_cast<uint8_t>(i);
        std::shuffle(s_perm.begin(), s_perm.end(), GetRng());
    }

    // スムースステップ（補間用）
    inline float FE_Smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

    // 1D の補間ノイズを返す（値域は -1..1 程度）
    float FE_Noise1D(float x)
    {
        // permutation が初期化済みかどうかを遅延初期化で保証
        static bool s_init = (FE_InitNoisePerm(), true);

        int xi = static_cast<int>(std::floor(x));
        float xf = x - static_cast<float>(xi);

        // ルックアップ（バイトラップ）
        uint8_t i0 = s_perm[static_cast<uint8_t>(xi & 0xFF)];
        uint8_t i1 = s_perm[static_cast<uint8_t>((xi + 1) & 0xFF)];

        // 0..255 を -1..1 にマップ
        float v0 = (static_cast<int>(i0) / 255.0f) * 2.0f - 1.0f;
        float v1 = (static_cast<int>(i1) / 255.0f) * 2.0f - 1.0f;

        float u = FE_Smoothstep(xf);
        // 線形補間（滑らかに接続）
        return v0 * (1.0f - u) + v1 * u;
    }

    // フラクタルノイズ（複数オクターブを合成）
    // octaves: 重ねる層数、lacunarity: 周波数倍率、gain: 振幅減衰
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
        // 正規化して -1..1 の範囲に収まるようにする（maxAmp が 0 にならない前提）
        return sum / maxAmp;
    }
}

namespace FearEffects
{
    // 揺れを開始する
    // intensity: 揺れの強さ、duration: 継続時間（秒）
    void StartShake(float intensity, float duration)
    {
        // マイナス入力を防ぐ
        g_shakeIntensity = std::max(0.0f, intensity);
        g_shakeDuration = std::max(0.0f, duration);
        g_shakeElapsed = 0.0f;

        // 位相をランダムにして、X/Y で異なる波形にする
        std::uniform_real_distribution<float> dist(0.0f, 3.14159265f * 2.0f);
        g_shakePhaseX = dist(GetRng());
        g_shakePhaseY = dist(GetRng());
    }

    // オーバーレイ（画面赤みなど）を開始する
    // stage: 見た目強度の段階（任意）
    void StartOverlay(float intensity, float duration, int stage)
    {
        g_overlayIntensity = std::max(0.0f, intensity);
        g_overlayDuration = std::max(0.0f, duration);   
        g_overlayElapsed = 0.0f;
        g_overlayStage = stage;
    }

    // 毎フレーム呼び出して経過時間を進める（dt: 秒）
    void Update(float dt)
    {
        if (g_shakeElapsed < g_shakeDuration) g_shakeElapsed = std::min(g_shakeElapsed + dt, g_shakeDuration);
        if (g_overlayElapsed < g_overlayDuration) g_overlayElapsed = std::min(g_overlayElapsed + dt, g_overlayDuration);
    }

    // 現在の揺れオフセットを取得（ImGui の SetNextWindowPos 等へ渡す）
    ImVec2 GetShakeOffset()
    {
        // 揺れが無効ならゼロを返す
        if (g_shakeDuration <= 0.0f || g_shakeElapsed >= g_shakeDuration) return ImVec2(0.0f, 0.0f);

        // 残り時間に応じたイージング（時間経過で徐々に収束）
        float rem = 1.0f - (g_shakeElapsed / g_shakeDuration);
        float ease = 1.0f - (1.0f - rem) * (1.0f - rem);
        float mag = g_shakeIntensity * ease;

        // 定常的な正弦成分（高速振動）とノイズ成分を混ぜる
        float sinX = std::sin(g_shakeElapsed * 23.0f + g_shakePhaseX);
        float sinY = std::cos(g_shakeElapsed * 19.0f + g_shakePhaseY);

        // フラクタルノイズで滑らかなランダム要素を加える
        float noiseX = FE_FractalNoise1D(g_shakeElapsed * 1.4f + g_shakePhaseX, 4);
        float noiseY = FE_FractalNoise1D(g_shakeElapsed * 1.1f + g_shakePhaseY, 4);

        const float sinWeight = 0.6f;   // 正弦成分の重み
        const float noiseWeight = 0.4f; // ノイズ成分の重み

        float x = mag * (sinWeight * sinX + noiseWeight * noiseX);
        float y = mag * (sinWeight * sinY + noiseWeight * noiseY);

        return ImVec2(x, y);
    }

    // オーバーレイを描画する（ImGui のフロントバッファへ直接描画）
    // GetMainViewport() を使って画面全体を覆う矩形を描く
    void RenderOverlay()
    {
        // 無効時は何もしない
        if (g_overlayDuration <= 0.0f || g_overlayElapsed >= g_overlayDuration) return;

        // 経過に応じてフェードアウトさせる
        float alpha = g_overlayIntensity * (1.0f - (g_overlayElapsed / g_overlayDuration));

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImGuiViewport* vp = ImGui::GetMainViewport();

        // 赤みを帯びた半透明矩形を画面全体に描画
        ImU32 col = ImGui::GetColorU32(ImVec4(0.6f, 0.0f, 0.0f, alpha));
        dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
    }
}