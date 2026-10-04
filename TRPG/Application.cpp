#include "Application.h"
#include <chrono>
#include <thread>
#include "system/imgui/imgui_impl_dx11.h"
#include "system/imgui/imgui.h"
#include "system/imgui/imgui_impl_win32.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "ImGuiFontLoader.h" // 追加
#include "Logging.h"
#include "AssetManager.h"
#include <sstream>
#include <system_error>
#include <typeinfo>

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

// Forward declaration so LogAndBreakException can be used in functions above its definition
static void LogAndBreakException(const std::exception& ex, const char* where);


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

    // Initialize logging subsystem and install global handlers
    Log::Initialize(2000);
    Log::InstallGlobalHandlers();

    // Install invalid parameter handler to capture CRT invalid-parameter errors
    // This prevents the CRT from calling abort() without useful diagnostics.
    _set_invalid_parameter_handler([](const wchar_t* expression,
                                      const wchar_t* function,
                                      const wchar_t* file,
                                      unsigned int line,
                                      uintptr_t pReserved) {
        // Compose a short message and send to debug output
        std::wstring msg = L"Invalid parameter detected:\n";
        if (expression) { msg += L"Expression: "; msg += expression; msg += L"\n"; }
        if (function) { msg += L"Function: "; msg += function; msg += L"\n"; }
        if (file) { msg += L"File: "; msg += file; msg += L"\n"; }
        {
            // OutputDebugStringW expects a null-terminated string
            OutputDebugStringW(msg.c_str());
        }
        // Trigger a debug break so developer can inspect call stack when a debugger is attached
        if (IsDebuggerPresent()) {
            __debugbreak();
        }
    });

    // TextureManager 初期化（AssetManager::GetAssetRoot を使って実行ファイル周りの assets を解決）
    std::string assetRoot = AssetManager::GetAssetRoot();
    {
        std::ostringstream ss; ss << "Application: asset root = " << assetRoot;
        Log::Log(Log::Level::Info, ss.str());
    }
    TextureManager::Initialize(m_Device, assetRoot);

    // フォントロード...
    bool fontOk = ImGuiFontLoader::InitializeImGuiFonts("resources/fonts/NotoSansJP-Regular.ttf");

    return true;
}


/**
 * @brief アプリ全体の終了処理
 */
void Application::TermApp()
{
    // TextureManager のキャッシュを解放
    TextureManager::Shutdown();

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

    // Enable drag & drop of files from Explorer
    DragAcceptFiles(m_hWnd, TRUE);

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
    using clock = std::chrono::steady_clock;
    const std::chrono::duration<double, std::milli> kTargetFrameTimeMs(1000.0 / 60.0); // 60 FPS

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            auto frameStart = clock::now();

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
            try {
                // Debug: log current scene type when in dev mode (SceneManager logs separately)
                g_SceneManager.Update();
            }
            catch (const std::exception& ex) {
                LogAndBreakException(ex, "Application: exception during SceneManager::Update");
            }
            catch (...) {
                Log::Log(Log::Level::Error, "Application: unknown exception during SceneManager::Update");
    // Break into debugger only if one is attached. Otherwise just return so program can continue
    if (IsDebuggerPresent()) {
        __debugbreak();
    }
            }

            try {
                g_SceneManager.Render();
            }
            catch (const std::exception& ex) {
                LogAndBreakException(ex, "Application: exception during SceneManager::Render");
            }
            catch (...) {
                Log::Log(Log::Level::Error, "Application: unknown exception during SceneManager::Render");
                if (IsDebuggerPresent()) __debugbreak();
            }
            // Update and Render are called once above inside try/catch blocks.
            // Do not call them again to avoid operating on destroyed scenes.

            // -----------------------------
            // 4. ImGui 描画
            // -----------------------------
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // -----------------------------
            // 5. 画面表示
            // -----------------------------
            m_SwapChain->Present(1, 0);

            // Frame pacing: sleep until target frame time is reached
            auto frameEnd = clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(frameEnd - frameStart);
            if (elapsed < kTargetFrameTimeMs) {
                auto sleepFor = kTargetFrameTimeMs - elapsed;
                // Sleep for the bulk, then spin for precision
                if (sleepFor > std::chrono::milliseconds(2))
                    std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(sleepFor) - std::chrono::milliseconds(1));
                while (std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(clock::now() - frameStart) < kTargetFrameTimeMs) {}
            }
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
    case WM_DROPFILES:
    {
        // Handle file dropped from Explorer (use wide APIs for Unicode paths)
        HDROP hDrop = (HDROP)wp;
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        if (fileCount > 0) {
            wchar_t wfilename[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, wfilename, MAX_PATH)) {
                // Convert wide path (UTF-16) to both UTF-8 for logging/UI and ANSI (system code page) for file APIs.
                // UTF-8 (for ImGui / internal logs)
                int reqUtf8 = ::WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, nullptr, 0, nullptr, nullptr);
                std::string utf8;
                if (reqUtf8 > 0) {
                    utf8.resize(reqUtf8);
                    int wroteUtf8 = ::WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, utf8.data(), reqUtf8, nullptr, nullptr);
                    if (wroteUtf8 > 0 && !utf8.empty() && utf8.back() == '\0') utf8.pop_back();
                }
                // ANSI (system code page) for file system APIs / fopen/ifstream
                int reqAnsi = ::WideCharToMultiByte(CP_ACP, 0, wfilename, -1, nullptr, 0, nullptr, nullptr);
                std::string ansi;
                if (reqAnsi > 0) {
                    ansi.resize(reqAnsi);
                    int wroteAnsi = ::WideCharToMultiByte(CP_ACP, 0, wfilename, -1, ansi.data(), reqAnsi, nullptr, nullptr);
                    if (wroteAnsi > 0 && !ansi.empty() && ansi.back() == '\0') ansi.pop_back();
                }
                // Prefer forwarding ANSI path to SceneManager since subsequent file checks use narrow APIs.
                if (!ansi.empty()) {
                    g_SceneManager.HandleFileDrop(ansi);
                } else if (!utf8.empty()) {
                    // fallback
                    g_SceneManager.HandleFileDrop(utf8);
                }
                // Also emit a UTF-8 log so the ImGui log shows a readable path
                if (!utf8.empty()) {
                    ::Log::Log(::Log::Level::Info, std::string("Dropped file: ") + utf8);
                }
            }
        }
        DragFinish(hDrop);
        return 0;
    }

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

// 既存の includes の直後あたりに追加
static void LogAndBreakException(const std::exception& ex, const char* where)
{
    try {
        std::ostringstream ss;
        ss << where << ": exception what=\"" << ex.what() << "\"";
        if (auto se = dynamic_cast<const std::system_error*>(&ex)) {
            ss << " | system_error.code=" << se->code().value()
               << " message=\"" << se->code().message() << "\"";
        }
        ss << " | type=" << typeid(ex).name();
        Log::Log(Log::Level::Error, ss.str());
    }
    catch (...) {
        Log::Log(Log::Level::Error, std::string(where) + ": failed to format exception details");
    }
    if (IsDebuggerPresent()) __debugbreak();
}

static std::string WideToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    if (size == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "WideCharToMultiByte");
    std::string s(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), s.data(), size, nullptr, nullptr);
    return s;
}
