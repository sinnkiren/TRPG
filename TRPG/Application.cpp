#include "Application.h"
#include <chrono>
#include <thread>
#include "system/imgui/imgui_impl_dx11.h"
#include "system/imgui/imgui.h"
#include "system/imgui/imgui_impl_win32.h"
#include "SceneManager.h"
#include "ImGuiFontLoader.h" // 追加

// 静的メンバ変数の定義
HINSTANCE  Application::m_hInst = nullptr;
HWND       Application::m_hWnd = nullptr;
uint32_t   Application::m_Width = 0;
uint32_t   Application::m_Height = 0;

uint32_t Application::GetWidth() {
    return m_Width;
}

uint32_t Application::GetHeight() {
    return m_Height;
}

HWND Application::GetWindow() {
    return m_hWnd;
}

HINSTANCE Application::GetHInstance() {
    return m_hInst;
}


ID3D11Device* Application::m_Device = nullptr;
ID3D11DeviceContext* Application::m_DeviceContext = nullptr;
IDXGISwapChain* Application::m_SwapChain = nullptr;
ID3D11RenderTargetView* Application::m_RenderTargetView = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


/**
 * @brief コンストラクタ
 * @details
 * - ウィンドウサイズを保持
 * - タイマー精度を 1ms に設定
 */
Application::Application(uint32_t width, uint32_t height)
{
    m_Width = width;
    m_Height = height;
    timeBeginPeriod(1);
}

/**
 * @brief デストラクタ
 * @details タイマー精度を元に戻す
 */
Application::~Application()
{
    timeEndPeriod(1);
}

/**
 * @brief アプリケーションの実行
 */
void Application::Run()
{
    // SceneManager 初期化
    g_SceneManager.Initialize();
    if (InitApp())   // 初期化に成功したら
    {
        MainLoop();  // メインループを実行
    }
    TermApp();       // 終了処理
}

/**
 * @brief アプリ全体の初期化
 */
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

    // ここで日本語フォントを読み込む（resources/fonts にフォントを置いている前提）
    // ファイル名は実際に配置したフォントファイル名に合わせてください。
    ImGuiFontLoader::InitializeImGuiFonts("resources/fonts/NotoSansJP-Regular.ttf");

    return true;
}


/**
 * @brief アプリ全体の終了処理
 */
void Application::TermApp()
{
    // DirectXリソース解放
    if (m_SwapChain) { m_SwapChain->Release(); m_SwapChain = nullptr; }
    if (m_DeviceContext) { m_DeviceContext->Release(); m_DeviceContext = nullptr; }
    if (m_Device) { m_Device->Release(); m_Device = nullptr; }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();


    TermWnd(); // ウィンドウ終了処理
}

/**
 * @brief ウィンドウ初期化
 */
bool Application::InitWnd()
{
    auto hInst = GetModuleHandle(nullptr);
    if (hInst == nullptr) return false;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(hInst, IDC_ARROW);
    wc.lpszClassName = TEXT("TRPGApp");

    if (!RegisterClassEx(&wc)) return false;

    m_hInst = hInst;

    RECT rc = { 0, 0, (LONG)m_Width, (LONG)m_Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    m_hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        TEXT("TRPG"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr, nullptr, m_hInst, nullptr
    );

    if (m_hWnd == nullptr) return false;

    ShowWindow(m_hWnd, SW_SHOWNORMAL);
    UpdateWindow(m_hWnd);

    return true;
}

/**
 * @brief ウィンドウ終了処理
 */
void Application::TermWnd()
{
    if (m_hInst) {
        UnregisterClass(TEXT("TRPGApp"), m_hInst);
        m_hInst = nullptr;
    }
    m_hWnd = nullptr;
}

/**
 * @brief メインループ
 */
void Application::MainLoop()
{
    MSG msg = {};

    // ImGui 初期化が済んでいる前提

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // -----------------------------
            // 1. 画面クリア
            // -----------------------------
            float clearColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
            m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clearColor);

            // -----------------------------
            // 2. ImGui 新フレーム開始
            // -----------------------------
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // -----------------------------
            // 3. シーン更新＆描画
            // -----------------------------
            g_SceneManager.Update();
            g_SceneManager.Render();

            // -----------------------------
            // 4. ImGui 描画
            // -----------------------------
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // -----------------------------
            // 5. 画面表示
            // -----------------------------
            m_SwapChain->Present(1, 0);
        }
    }
}

/**
 * @brief ウィンドウプロシージャ
 */
LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
    break;

    default:
    { /* DO_NOTHING */ }
    break;
    }

    return DefWindowProc(hWnd, msg, wp, lp);
}

// 既存の static メンバ定義の直後に追加（ファイル先頭付近にまとめると良いです）
ID3D11Device* Application::GetDevice()
{
    return m_Device;
}

ID3D11DeviceContext* Application::GetDeviceContext()
{
    return m_DeviceContext;
}
