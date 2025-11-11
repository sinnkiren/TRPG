#pragma once
#include "IScene.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

class CharcterScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;

    struct CharcterDate
    {
        std::string name;
        std::string job;

        int str =0, con =0, dex =0, int_ =0, pow =0, cha =0, app =0, siz =0, edu =0; // 能力値
        std::vector<std::string> skills; // 技能

        int sanity =100; //旧: 正気度（互換のため残す）
        int maxSanity =100; //旧: 最大正気度

        // 新: 耐久力（HP）
        int endurance =0;
        int maxEndurance = 0;

        bool ApplySanityLoss(int amount) {
            sanity = std::clamp(sanity - amount,0, maxSanity);
            return sanity ==0;
        }
        void RecoverSanity(int amount) {
            sanity = std::clamp(sanity + amount,0, maxSanity);
        }

        bool ApplyEnduranceLoss(int amount) {
            endurance = std::clamp(endurance - amount,0, maxEndurance);
            return endurance ==0;
        }
        void RecoverEndurance(int amount) {
            endurance = std::clamp(endurance + amount,0, maxEndurance);
        }
    };

    // シーンマネージャへシーン遷移要求を出すコールバック
    std::function<void(int)> RequestSceneChange;

private:
    CharcterDate charcter;

    // サンプルキャラのリスト
    std::vector<CharcterDate> sampleCharacters;
    int selectedSampleIndex = -1;

    // UI補助: 最後に振ったダイス
    int lastDiceRoll =0;

    // 能力値行（表示とダイス式、ロック等）
    struct AbilityRow {
        std::string name;
        std::string expr; //例: "3D6", "2D6+6"
        int value =0; // 現在の値
        bool locked = false; // ロックされていれば一括振りで上書きしない
    };

    std::vector<AbilityRow> abilities;
};