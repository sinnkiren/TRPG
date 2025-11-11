#pragma once
#include <random>
#include <vector>

// 単純なダイスロールユーティリティ
namespace Dice
{
    inline std::mt19937& GetEngine()
    {
        static std::random_device rd;
        static std::mt19937 eng(rd());
        return eng;
    }

    // 1..sides の単一ダイス
    inline int RollDie(int sides)
    {
        std::uniform_int_distribution<int> dist(1, std::max(1, sides));
        return dist(GetEngine());
    }

    // count 個の sides 面ダイスをロールして合計を返す
    inline int RollDice(int count, int sides)
    {
        int sum = 0;
        for (int i = 0; i < std::max(1, count); ++i) sum += RollDie(sides);
        return sum;
    }
}