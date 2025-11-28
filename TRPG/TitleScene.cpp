#include "TitleScene.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"

// タイトルフェード用ステート（ウィンドウ内表示）
static float s_titleAlpha = 0.0f;
static float s_titleFadeInDuration = 1.0f;
static float s_titleFadeOutDuration = 0.8f;
static float s_titleFadeElapsed = 0.0f;
static bool  s_fadingIn = true;
static bool  s_fadingOut = false;

// 画面全体フェード用ステート（フルスクリーン）
static float s_fsFadeAlpha = 0.0f;
static float s_fsFadeDurationIn = 1.0f;   // 起動時に黒→画面 の時間
static float s_fsFadeDurationOut = 0.8f;  // 終了時に画面→黒 の時間
static float s_fsFadeElapsed = 0.0f;
static bool  s_fsFadingOut = false;
static bool  s_fsFadingIn = true;

void TitleScene::Initialize()
{
    // ウィンドウ内タイトルフェード
    s_titleAlpha = 0.0f;
    s_titleFadeElapsed = 0.0f;
    s_fadingIn = true;
    s_fadingOut = false;

    // フルスクリーンフェード（起動時は黒→表示）
    s_fsFadeAlpha = 1.0f; // 黒で覆う状態からスタート
    s_fsFadeElapsed = 0.0f;
    s_fsFadingIn = true;
    s_fsFadingOut = false;
}

void TitleScene::Update()
{
    float dt = ImGui::GetIO().DeltaTime;

    // ウィンドウ内タイトルフェード更新（既存挙動）
    if (s_fadingIn) {
        s_titleFadeElapsed += dt;
        float r = (s_titleFadeInDuration > 0.0f) ? (s_titleFadeElapsed / s_titleFadeInDuration) : 1.0f;
        s_titleAlpha = (r >= 1.0f) ? 1.0f : (r < 0.0f ? 0.0f : r);
        if (r >= 1.0f) s_fadingIn = false;
    } else if (s_fadingOut) {
        s_titleFadeElapsed += dt;
        float r = (s_titleFadeOutDuration > 0.0f) ? (s_titleFadeElapsed / s_titleFadeOutDuration) : 1.0f;
        s_titleAlpha = 1.0f - (r >= 1.0f ? 1.0f : (r < 0.0f ? 0.0f : r));
        if (r >= 1.0f) {
            s_fadingOut = false;
            s_titleAlpha = 0.0f;
        }
    }

    // フルスクリーンフェード（起動時フェードイン：黒 -> 透明）
    if (s_fsFadingIn) {
        s_fsFadeElapsed += dt;
        float r = (s_fsFadeDurationIn > 0.0f) ? (s_fsFadeElapsed / s_fsFadeDurationIn) : 1.0f;
        s_fsFadeAlpha = 1.0f - std::clamp(r, 0.0f, 1.0f);
        if (r >= 1.0f) {
            s_fsFadingIn = false;
            s_fsFadeAlpha = 0.0f;
        }
    }

    // フルスクリーンフェードアウト（Start 押下で開始）
    if (s_fsFadingOut) {
        s_fsFadeElapsed += dt;
        float r = (s_fsFadeDurationOut > 0.0f) ? (s_fsFadeElapsed / s_fsFadeDurationOut) : 1.0f;
        s_fsFadeAlpha = std::clamp(r, 0.0f, 1.0f);
        if (r >= 1.0f) {
            s_fsFadingOut = false;
            s_fsFadeAlpha = 1.0f;
            // フェードアウト完了：シーン切替（プロジェクトの API に合わせて修正）
            g_SceneManager.ChangeScene(SceneType::TRPG_SELECT);
        }
    }
}

void TitleScene::Render()
{
    ImGui::Begin("Title");

    // 中央に大きなタイトルを描画（アルファ適用）
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("MY GAME TITLE").x) * 0.5f);
    float a = s_titleAlpha;
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.6f, a), "MY GAME TITLE");

    ImGui::Spacing();

    // 開始ボタン（押したらフルスクリーン フェードアウトを開始）
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Start").x) * 0.5f);
    if (ImGui::Button("Start")) {
        // ウィンドウ内タイトルもフェードアウトさせる（任意）
        s_fadingOut = true;
        s_titleFadeElapsed = 0.0f;
        // フルスクリーンのフェードアウトを開始
        s_fsFadingOut = true;
        s_fsFadingIn = false;
        s_fsFadeElapsed = 0.0f;
    }

    ImGui::End();

    // フルスクリーン矩形をフォアグラウンドに描画して画面全体をフェードさせる
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (dl) {
            // 黒で覆う。alpha = s_fsFadeAlpha（0: 透明, 1: 黒）
            ImU32 col = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, s_fsFadeAlpha));
            dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), col);
        }
    }
}