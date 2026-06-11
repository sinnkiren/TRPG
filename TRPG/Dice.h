#pragma once
#include <random>
#include <vector>
#include <algorithm>
#include "DiceVisual.h"

// 単純なダイスロールユーティリティ
namespace Dice
{
    inline std::mt19937& GetEngine()
    {
        static std::random_device rd;
        static std::mt19937 eng(rd());
        return eng;
    }

    // Roll without visual side-effect (useful when rolling multiple dice and
    // wanting to trigger visual only once)
    inline int RollDieNoVisual(int sides)
    {
        std::uniform_int_distribution<int> dist(1, std::max(1, sides));
        return dist(GetEngine());
    }

    // 1..sides の単一ダイス
    inline int RollDie(int sides)
    {
        int r = RollDieNoVisual(sides);
        // trigger visual feedback (non-blocking)
        DiceVisual::Instance().StartRoll(sides, r);
        return r;
    }

    // Roll percentile (00-99 -> 1..100): returns 1..100 where 00 maps to 100.
    // Triggers a two-d10 visual: first die is tens digit, second is ones digit.
    inline int RollPercentile()
    {
        // roll two d10 without visuals
        int r1 = RollDieNoVisual(10); // 1..10 (10 represents 0)
        int r2 = RollDieNoVisual(10);
        int tens = r1 % 10; // maps 10->0
        int ones = r2 % 10;
        int val = tens * 10 + ones; // 0..99
        if (val == 0) val = 100;
        // trigger visual for both dice (faces use 1..10 encoding, with 10 representing 0)
        std::vector<int> faces;
        faces.push_back(r1);
        faces.push_back(r2);
        DiceVisual::Instance().StartRollFaces(10, faces);
        return val;
    }

    // count 個の sides 面ダイスをロールして合計を返す
    inline int RollDice(int count, int sides)
    {
        int sum = 0;
        for (int i = 0; i < std::max(1, count); ++i) sum += RollDie(sides);
        return sum;
    }
}