#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include "IScene.h"
#include "system/json.hpp"
#include "system/imgui/imgui.h"
#include <d3d11.h>
#include <chrono>

using json = nlohmann::json;

struct StoryEvent
{
	std::string text;
	std::string speaking;
	std::string faceImage;
	std::string effect;
	float duration = 1.0f;
	json effectParams = nullptr; // JSON での効果パラメータを保持
};
class StoryPlayer : public IScene {
	using EffectHandler = std::function<void(const StoryEvent)>;
public:
	StoryPlayer();
	~StoryPlayer();
	void Initialize() override;
	void Update() override;   // 未使用
	void Render() override;
	void RenderUI() override;

	void UpdateImpl(float dt); // float dt を受け取る更新

	bool LoadFromFile(const std::string& path);
	bool SaveToFile(const std::string& path) const;
	void Play();//再生
	void Pause();//一時停止
	void Next();//次へ
	void Reset();//リセット

	// エフェクト登録 API
	void RegisterEffect(const std::string& name, EffectHandler  handler);
	// イベント完了コールバック
	std::function<void(const StoryEvent&)>onEventFinished;

	// 背景画像設定（汎用 ImTextureID）
	void SetBackgroundTexture(ImTextureID tex) { m_bgTex = tex; }
	ImTextureID GetBackgroundTexture() const { return m_bgTex; }

#ifdef IMGUI_IMPL_DIRECTX11
	// DirectX11 の SRV を渡すヘルパー（StoryPlayer が既存 SRV を解放して管理します）
	void SetBackgroundSRV(ID3D11ShaderResourceView* srv);
#endif

private:
	std::vector<StoryEvent>m_events;
	int m_index = 0;
	float m_timer = 0.0f;
	bool m_playing = false;

	std::unordered_map < std::string, EffectHandler>m_effects;
	void TriggerEffect(const StoryEvent& ev);
	void ShowCurrentText();

	// 追加メンバ
	float m_textScale = 1.0f;      // 文字の拡大率（1.0 = 標準）
	ImVec2 m_dialogSize = ImVec2(400.0f, 160.0f); // ダイアログサイズのデフォルト

	// フェード遷移用（story -> battle）
	float m_fsFadeAlpha = 0.0f;
	float m_fsFadeDuration = 0.8f;
	float m_fsFadeElapsed = 0.0f;
	bool  m_fsFadingOut = false;

	// 背景テクスチャ（ImGui 用）
	ImTextureID m_bgTex = nullptr;

#ifdef IMGUI_IMPL_DIRECTX11
	// DirectX11 用に SRV の参照を保持して自動解放
	ID3D11ShaderResourceView* m_bgSrv = nullptr;
#endif

	// 遅延ロード用メンバ（追加）
	std::string m_bgPath;             // 読み込む画像のパス（遅延読み込み用）
	bool m_bgLoadedAttempted = false; // 一度読み込み試行済みフラグ

	// 内部ヘルパー（遅延読み込み）
	void LoadBackgroundTextureIfNeeded();

	// フェード開始
	void StartFadeToBattle(float duration = 0.8f);

    // 最後のロード/保存エラーを保持（UI表示用）
    std::string m_lastLoadError;

    // 遅延ロード再試行制御
    int m_bgLoadAttempts = 0;          // 試行回数
    int m_bgMaxLoadAttempts = 8;       // 最大試行回数（-1 で無制限）
    float m_bgLastAttemptTime = 0.0f;  // 最終試行時刻 (ImGui::GetTime())
    float m_bgRetryInterval = 2.0f;    // 再試行間隔（秒）

	// Dev UI: toggle for overlay/debug window
	bool m_showDevWindow = false;
    // Dev helper: node jump input
    int m_devNodeInput = 0;
    // Expose some info to centralized dev panel
public:
    void RenderDevPanelContents();
};