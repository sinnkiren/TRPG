#include "Application.h"
#include <chrono>
#include <thread>
#include "system/imgui/imgui_impl_dx11.h"
#include "system/imgui/imgui.h"
#include "system/imgui/imgui_impl_win32.h"
#include "SceneManager.h"
#include "ImGuiFontLoader.h" // 追加

bool Application::InitApp()
{
    // 1. ウィンドウ作成
    if (!InitWnd()) return false;

    // 2. DirectX11 初期化
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = m_Width;
    sd.BufferDesc.Height = m_Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = {};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &m_SwapChain,
        &m_Device,
        &featureLevel,
        &m_DeviceContext
    );

    if (FAILED(hr)) {
        MessageBox(m_hWnd, L"DirectX11の初期化に失敗しました", L"エラー", MB_OK);
        return false;
    }

    // バックバッファ取得
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr) || !pBackBuffer) {
        MessageBox(m_hWnd, L"バックバッファの取得に失敗しました", L"エラー", MB_OK);
        return false;
    }

    // レンダーターゲットビュー作成
    hr = m_Device->CreateRenderTargetView(pBackBuffer, nullptr, &m_RenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr) || !m_RenderTargetView) {
        MessageBox(m_hWnd, L"レンダーターゲットビューの作成に失敗しました", L"エラー", MB_OK);
        return false;
    }

    // 出力先に設定
    m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);

    // 3. ImGui 初期化（DirectX デバイスが作成された後）
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_Device, m_DeviceContext);

    // --- ここで日本語フォント読み込みを呼ぶ（相対パス or 絶対パスを指定） ---
    // 実行時のカレントディレクトリがビルド出力フォルダ（例: Debug/）であることに注意。
    // resources/fonts/NotoSansJP-Regular.ttf を配置していることを確認してください。
    bool fontOk = ImGuiFontLoader::InitializeImGuiFonts("resources/fonts/NotoSansJP-Regular.ttf");
    if (!fontOk) {
        MessageBoxA(nullptr, "ImGui Japanese font load failed. Check resources/fonts path or use absolute path.", "Font Load", MB_OK | MB_ICONWARNING);
    }

    return true;
}