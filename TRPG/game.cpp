#include "game.h"
#include "system/imgui/imgui.h"
#include "system/imgui/imgui_impl_dx11.h"
#include "system/imgui/imgui_impl_win32.h"

static SceneManager g_SceneManager;

void gameinit(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    g_SceneManager.Initialize(); // シーン初期化

    // ImGui 初期化
    ImGui::CreateContext();                         // コンテキスト作成
    ImGuiIO& io = ImGui::GetIO();                   // 入力設定など
    ImGui::StyleColorsDark();                       // スタイル設定（任意）

    ImGui_ImplWin32_Init(hwnd);                     // Win32連携
    ImGui_ImplDX11_Init(device, deviceContext);     // DirectX11連携

}

void gameloop()
{
    // ImGui フレーム開始
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // シーン更新・描画（ImGui::Button などを含む）
    g_SceneManager.Update();
    g_SceneManager.Render();

    // ImGui 描画
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void gamedispose()
{
    g_SceneManager.Finalize(); // シーンの後処理

    // ImGui 終了処理
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}