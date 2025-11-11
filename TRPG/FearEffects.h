#pragma once
#include "system/imgui/imgui.h"
#include <cmath>

// 簡易恐怖オーバーレイ（ImGui の ForegroundDrawList を利用）
// stage:0..4 のダメージ段階（0: 軽微、4: 致命的）
inline void DrawFearOverlay(float intensity, float timeSeconds, int stage =0)
{
    if (intensity <=0.001f) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 disp = ImGui::GetIO().DisplaySize;

    // stage によって効果の強さや色味を変える
    // stageBoost は全体の透明度を増す係数
    float stageBoost =0.5f +0.3f * stage; //0.5 ..1.7
    float redTint =0.2f +0.2f * stage; //0.2 ..1.0

    // 全体を薄暗くする（段階に応じて強める）
    ImU32 dark = ImColor(0.0f,0.0f,0.0f,0.15f * intensity * stageBoost);
    dl->AddRectFilled(ImVec2(0,0), disp, dark);

    // 中心を少し明るく、周辺を赤みがかったヴィネット
    for (int i =0; i <3; ++i) {
        float pad = i *40.0f * intensity * (1.0f +0.2f * stage);
        float a =0.02f +0.06f * intensity * (1.0f +0.6f * stage) * (1.0f +0.5f * std::sin(timeSeconds * (1.2f + i)));
        // 赤の成分を段階的に上げる
        ImU32 col = ImColor(std::min(1.0f, redTint +0.1f * i),0.0f,0.0f, a);
        dl->AddRectFilled(ImVec2(pad, pad), ImVec2(disp.x - pad, disp.y - pad), col);
    }

    //低強度の白ノイズ点を散らす（不穏感）。ステージが高いほど数や不透明度を増やす
    ImU32 noise = ImColor(1.0f,1.0f,1.0f,0.005f * intensity * (1.0f +0.4f * stage));
    int noiseCount =20 + stage *15;
    for (int i =0; i < noiseCount; ++i) {
        float x = fmodf(timeSeconds * (13.0f + i *3.7f), disp.x);
        float y = fmodf(timeSeconds * (19.0f + i *2.9f), disp.y);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x +2.0f, y +2.0f), noise);
    }
}