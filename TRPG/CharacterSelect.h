#pragma once
#include "IScene.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

namespace trpg
{
    // 簡易 clamp（環境によって std::clamp が見つからない問題の回避用）
    template <typename T>
    constexpr const T& clamp(const T& v, const T& lo, const T& hi) noexcept
    {
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }
}

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
        // Path to portrait image (relative to asset root or absolute)
        std::string portraitPath;

        bool ApplySanityLoss(int amount) {
            sanity = static_cast<int>(trpg::clamp(sanity - amount, 0, maxSanity));
            return sanity ==0;
        }
        void RecoverSanity(int amount) {
            sanity = static_cast<int>(trpg::clamp(sanity + amount, 0, maxSanity));
        }

        bool ApplyEnduranceLoss(int amount) {
            endurance = static_cast<int>(trpg::clamp(endurance - amount, 0, maxEndurance));
            return endurance ==0;
        }
        void RecoverEndurance(int amount) {
            endurance = static_cast<int>(trpg::clamp(endurance + amount, 0, maxEndurance));
        }
    };

    // シーンマネージャへシーン遷移要求を出すコールバック
    std::function<void(int)> RequestSceneChange;

    // Dev-only helper to set portrait path from external input (drag & drop)
    void SetPortraitPath(const std::string& path);

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
    // Helpers to sync abilities <-> character fields
    void RefreshAbilitiesFromCharacter();
    void ApplyAbilitiesToCharacter();
};