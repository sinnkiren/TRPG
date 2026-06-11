#pragma once
#include "IScene.h"

class ExploreScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;
    void RenderUI() override;
    // Expose save/load so other scenes can persist players/state immediately
    static void SaveStateNow();
    static void LoadStateNow();
};

// 注意: ExploreScene の SaveStateNow / LoadStateNow は内部の SaveExploreState / LoadExploreState を
// ラップしています。外部からプレイヤーデータや探索状態を即時保存/読み込みしたい場合に使用してください。
