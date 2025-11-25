#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include "IScene.h"
#include "system/json.hpp"
#include "system/imgui/imgui.h"

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
class StoryPlayer : public IScene{
	using EffectHandler = std::function<void(const StoryEvent)>;
public:
	StoryPlayer();
	~StoryPlayer();
		void Initialize() override;
		void Update() override;   // 未使用
		void Render() override;

		void UpdateImpl(float dt); // float dt を受け取る更新

	bool LoadFromFile(const std::string& path);
	void Play();//再生
	void Pause();//一時停止
	void Next();//次へ
	void Reset();//リセット

	// エフェクト登録 API
	void RegisterEffect(const std::string& name, EffectHandler  handler);
	// イベント完了コールバック
	std::function<void(const StoryEvent&)>onEventFinished;

	// 追加: ダイアログ／文字サイズ調整 API
	void SetTextScale(float scale) { m_textScale = std::max(0.1f, scale); }
	float GetTextScale() const { return m_textScale; }

	void SetDialogSize(const ImVec2& size) { m_dialogSize = ImVec2(std::max(1.0f, size.x), std::max(1.0f, size.y)); }
	ImVec2 GetDialogSize() const { return m_dialogSize; }

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
};