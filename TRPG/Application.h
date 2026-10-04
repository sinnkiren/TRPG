#pragma once

#pragma comment(lib, "winmm.lib")

#include <Windows.h>
#include <cstdint>
#include <d3d11.h>
#include "system/NonCopyable.h"

/**
 * @brief アプリケーションクラスです.
 * @details
 * - アプリ全体の初期化・終了処理・メインループを管理
 * - DirectX11 デバイスやウィンドウハンドルを保持
 * - 非コピー可能にするために NonCopyable を継承
 */
class Application : NonCopyable
{
public:
    Application(uint32_t width, uint32_t height);  ///< コンストラクタ（ウィンドウサイズを指定）
    ~Application();                               ///< デストラクタ（リソース解放）

    void Run();                                   ///< アプリケーションの実行（メインループ開始）

    /// ウィンドウ情報のゲッター
    static uint32_t GetWidth();    ///< ウィンドウの横幅
    static uint32_t GetHeight();   ///< ウィンドウの縦幅
    static HWND GetWindow();       ///< ウィンドウハンドル
    static HINSTANCE GetHInstance(); ///< インスタンスハンドル

    // DirectX デバイス取得用アクセサ（外部から直接 m_Device へ触らない）
    static ID3D11Device* GetDevice();
    static ID3D11DeviceContext* GetDeviceContext();

private:
    // ========== メンバ変数 ==========
    static HINSTANCE   m_hInst;    ///< インスタンスハンドル
    static HWND        m_hWnd;     ///< ウィンドウハンドル
    static uint32_t    m_Width;    ///< ウィンドウ横幅
    static uint32_t    m_Height;   ///< ウィンドウ縦幅

    // DirectX11 の主要なインターフェース
    static ID3D11Device* m_Device;             ///< Direct3D デバイス
    static ID3D11DeviceContext* m_DeviceContext; ///< デバイスコンテキスト
    static IDXGISwapChain* m_SwapChain;       ///< スワップチェーン（画面の裏表切り替え）
    static ID3D11RenderTargetView* m_RenderTargetView; // レンダーターゲットビュー

    // ========== 内部関数 ==========
    static bool InitApp();   ///< アプリ全体の初期化
    static void TermApp();   ///< アプリ全体の終了処理
    static bool InitWnd();   ///< ウィンドウの初期化
    static void TermWnd();   ///< ウィンドウの終了処理
    static void MainLoop();  ///< メインループ

    // ウィンドウプロシージャ（イベント処理）
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
};
