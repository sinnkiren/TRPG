#pragma once
#include <vector>
#include <string>
#include <functional>
#include "CharacterSelect.h"
#include "BattleScene.h"

namespace BattleLogic {
    using PushLog = std::function<void(const std::string&, int)>;

    // Player attack logic: roll d100, determine success and damage, update target/player/phase
    void PlayerAttack(BattleScene::Enemy& target,
                      CharcterScene::CharcterDate& player,
                      int& lastRoll,
                      BattleScene::Phase& phase,
                      PushLog pushLog);

    // Execute a single enemy turn (simple AI)
    void ExecuteEnemyTurn(std::vector<BattleScene::Enemy>& enemies,
                          CharcterScene::CharcterDate& player,
                          PushLog pushLog);

    // Advance/update turn: if phase==EnemyTurn, run enemy actions and flip phase
    void UpdateTurn(std::vector<BattleScene::Enemy>& enemies,
                    CharcterScene::CharcterDate& player,
                    BattleScene::Phase& phase,
                    PushLog pushLog);

    // Victory/defeat checks
    bool CheckVictory(const std::vector<BattleScene::Enemy>& enemies);
    bool CheckDefeat(const CharcterScene::CharcterDate& player);
}
