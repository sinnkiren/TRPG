#include "TitleScene.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include <random>
#include <algorithm>

// Local helpers ? avoid relying on std::min/std::max/std::clamp which may be
// unavailable or macro-shadowed in some build environments.
template<typename T>
static inline T clamp_t(T v, T lo, T hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }
template<typename T>
static inline T max_t(T a, T b) { return (a > b) ? a : b; }
template<typename T>
static inline T min_t(T a, T b) { return (a < b) ? a : b; }

// ===== フェード関連 =====
static float s_titleAlpha = 0.0f;
static float s_titleFadeInDuration = 1.0f;
static float s_titleFadeOutDuration = 0.8f;
static float s_titleFadeElapsed = 0.0f;
static bool  s_fadingIn = true;
static bool  s_fadingOut = false;

// ===== フルスクリーンフェード =====
static float s_fsFadeAlpha = 0.0f;
static float s_fsFadeDurationIn = 1.0f;
static float s_fsFadeDurationOut = 0.8f;
static float s_fsFadeElapsed = 0.0f;
static bool  s_fsFadingOut = false;
static bool  s_fsFadingIn = true;

// ===== タイトル画像 =====
static ImTextureID s_titleTex = nullptr;
static bool s_titleLoadAttempted = false;

// ===== ノイズ演出 =====
static bool s_noiseActive = false;
static float s_noiseDuration = 1.0f;
static float s_noiseElapsed = 0.0f;
static unsigned int s_noiseSeed = 0;
static float s_noiseIntensity = 1.0f;


void TitleScene::Initialize()
{
    // フェード初期化
    s_titleAlpha = 0.0f;
    s_titleFadeElapsed = 0.0f;
    s_fadingIn = true;
    s_fadingOut = false;

    // フルスクリーンフェード（起動時は黒→表示）
    s_fsFadeAlpha = 1.0f;
    s_fsFadeElapsed = 0.0f;
    s_fsFadingIn = true;
    s_fsFadingOut = false;

    // 画像は Render 時に遅延ロードする（TextureManager が初期化済みであることを期待）
    s_titleTex = nullptr;
    s_titleLoadAttempted = false;

    // ノイズ状態リセット
    s_noiseActive = false;
    s_noiseElapsed = 0.0f;
    s_noiseSeed = 0;
}

void TitleScene::Update()
{
    float dt = ImGui::GetIO().DeltaTime;

    if (s_fadingIn) {
        s_titleFadeElapsed += dt;
        float r = (s_titleFadeInDuration > 0.0f) ? (s_titleFadeElapsed / s_titleFadeInDuration) : 1.0f;
        s_titleAlpha = std::clamp(r, 0.0f, 1.0f);
        if (r >= 1.0f) s_fadingIn = false;
    }
    else if (s_fadingOut) {
        s_titleFadeElapsed += dt;
        float r = (s_titleFadeOutDuration > 0.0f) ? (s_titleFadeElapsed / s_titleFadeOutDuration) : 1.0f;
        s_titleAlpha = 1.0f - std::clamp(r, 0.0f, 1.0f);
        if (r >= 1.0f) {
            s_fadingOut = false;
            s_titleAlpha = 0.0f;
        }
    }

    // ノイズタイマー（ノイズ完了後にフェードアウトを開始）
    if (s_noiseActive) {
        s_noiseElapsed += dt;
        if (s_noiseElapsed >= s_noiseDuration) {
            s_noiseActive = false;
            s_noiseElapsed = 0.0f;
            // ノイズ終了 → フルスクリーンフェードアウト開始
            s_fsFadingOut = true;
            s_fsFadingIn = false;
            s_fsFadeElapsed = 0.0f;
            // 画面内タイトルもフェードアウトさせる
            s_fadingOut = true;
            s_titleFadeElapsed = 0.0f;
        }
    }

    if (s_fsFadingIn) {
        s_fsFadeElapsed += dt;
        float r = (s_fsFadeDurationIn > 0.0f) ? (s_fsFadeElapsed / s_fsFadeDurationIn) : 1.0f;
        s_fsFadeAlpha = 1.0f - std::clamp(r, 0.0f, 1.0f);
        if (r >= 1.0f) { s_fsFadingIn = false; s_fsFadeAlpha = 0.0f; }
    }

    if (s_fsFadingOut) {
        s_fsFadeElapsed += dt;
        float r = (s_fsFadeDurationOut > 0.0f) ? (s_fsFadeElapsed / s_fsFadeDurationOut) : 1.0f;
        s_fsFadeAlpha = std::clamp(r, 0.0f, 1.0f);
        if (r >= 1.0f) {
            s_fsFadingOut = false;
            s_fsFadeAlpha = 1.0f;
            g_SceneManager.ChangeScene(SceneType::TRPG_SELECT);
        }
    }
}

void TitleScene::Render()
{
    // フルスクリーンで画像のみ表示する実装。
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (!vp) return;

    // 遅延ロード（TextureManager が初期化されているタイミングで試行）
    if (!s_titleTex && !s_titleLoadAttempted) {
        s_titleLoadAttempted = true;
        s_titleTex = TextureManager::GetImGuiTextureID("texture/dark-tunnel2.png");
        if (!s_titleTex) s_titleTex = TextureManager::GetImGuiTextureID("texture/dark-tunnel2.png");
        if (!s_titleTex) OutputDebugStringA("TitleScene: title image not found: texturedark-tunnel2.png/.jpg\n");
    }

    // 背景レイヤに画像をフルスクリーンで描画（縦横比は画像により伸縮される）
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    if (bg) {
        if (s_titleTex) {
            bg->AddImage(s_titleTex, vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
        }
        else {
            ImU32 clearCol = ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
            bg->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), clearCol);
        }
    }

    // ---- ノイズ描画（前景レイヤ） ----
    if (s_noiseActive) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        if (fg) {
            // ノイズパラメータ
            float progress = s_noiseElapsed / max_t(0.0001f, s_noiseDuration);
            float inv = 1.0f - progress;
            float alphaBase = s_noiseIntensity * inv; // 徐々に弱まる
            int screenW = (int)vp->Size.x;
            int screenH = (int)vp->Size.y;

            // 描画数は画面サイズに合わせてスケール（重すぎない程度に抑える）
            int approxCells = (int)clamp_t((screenW * screenH) / (128 * 128), 80, 800);
            std::mt19937 rng(s_noiseSeed + (unsigned int)(s_noiseElapsed * 1000.0f));
            std::uniform_int_distribution<int> dx(0, screenW - 1);
            std::uniform_int_distribution<int> dy(0, screenH - 1);
            std::uniform_real_distribution<float> ds(1.0f, 12.0f);
            std::uniform_real_distribution<float> da(0.2f, 1.0f);

            for (int i = 0; i < approxCells; ++i) {
                int x = dx(rng);
                int y = dy(rng);
                float size = ds(rng);
                float a = da(rng) * alphaBase;
                ImU32 col = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, a));
                fg->AddRectFilled(ImVec2(vp->Pos.x + x, vp->Pos.y + y),
                    ImVec2(vp->Pos.x + x + size, vp->Pos.y + y + size),
                    col);
            }
        }
    }

    // 入力: 画面どこかをクリックしたらノイズ→フェードのシーケンスを開始
    if (!s_noiseActive && !s_fsFadingOut && ImGui::IsMouseClicked(0)) {
        s_noiseActive = true;
        s_noiseElapsed = 0.0f;
        // ランダム種を生成（毎回異なるノイズ）
        std::random_device rd;
        s_noiseSeed = rd();
    }

    // フルスクリーンの黒矩形でフェード（最前景）
    ImDrawList* fg2 = ImGui::GetForegroundDrawList();
    if (fg2) {
        ImU32 col = ImGui::GetColorU32(ImVec4(0, 0, 0, s_fsFadeAlpha));
        fg2->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
    }

    // Dev mode toggle (runtime) - small overlay in the top-right corner
    // Allows switching dev/play modes without rebuilding.
    if (ImGui::GetCurrentContext() != nullptr) {
        ImVec2 winPos(vp->Pos.x + vp->Size.x - 220.0f, vp->Pos.y + 8.0f);
        ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("DevToggle", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        bool dev = g_SceneManager.IsDevMode();
        if (ImGui::Checkbox("Dev Mode", &dev)) {
            g_SceneManager.SetDevMode(dev);
        }
        ImGui::End();
    }
}