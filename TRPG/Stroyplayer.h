#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct StoryEvent
{
	std::string text;
	std::string speaking;
	std::string faceImage;
	std::string effect;
	float duration = 1.0f;
};
class StoryPlayer {
	using EffectHandler = std::function<void(const StoryEvent)>;
public:
	StoryPlayer();
	~StoryPlayer();

	bool LoadFromFile(const std::string& path);
	void Update(float dt);//毎フレーム呼ぶ
	void Render();//テキスト描画
	void Play();//再生開始
	void Pause();//一時停止
	void Next();//次のイベントへ強制移行
	void Reset();//

	//外部kらエフェクトのハンドラ登録
	void RegisterEffect(const std::string& name, EffectHandler  handler);
	//イベント完了時のコールバック
	std::function<void(const StoryEvent&)>onEventFinished;

private:
	std::vector<StoryEvent>m_events;
	int m_index = 0;
	float m_timer = 0.0f;
	bool m_playing = false;

	std::unordered_map < std::string, EffectHandler>m_effects;
	void TriggerEffect(const StoryEvent& ev);
	void ShowCurrentText();
};