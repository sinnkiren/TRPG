#include "BattleLogic.h"
#include "Dice.h"
#include <algorithm>
#include "Logging.h"

namespace BattleLogic {

    void PlayerAttack(BattleScene::Enemy& target,
                      CharcterScene::CharcterDate& player,
                      int& lastRoll,
                      BattleScene::Phase& phase,
                      PushLog pushLog)
    {
        // roll percentile using two d10 visuals (tens and ones)
        lastRoll = Dice::RollPercentile();
        bool success = (lastRoll <= player.pow * 5);
        if (success) {
            // roll damage without triggering an extra single-die visual (keep percentile visual visible)
            int dmg = 4 + Dice::RollDieNoVisual(3); // 4 + d3
            target.hp = std::max(0, target.hp - dmg);
            if (pushLog) pushLog(player.name + " ÇÕ " + target.name + " Ç… " + std::to_string(dmg) + " ÇÃÉ_ÉÅÅ[ÉWÇó^Ç¶ÇΩÅB", 1);
            if (target.hp == 0 && pushLog) pushLog(target.name + " Çì|ÇµÇΩÅI", 1);
        }
        else {
            int dmg = target.fearDamage;
            player.ApplyEnduranceLoss(dmg);
            if (pushLog) pushLog(player.name + " ÇÃçUåÇÇÕé∏îsÇµÇΩÅBëœãvóÕÇ™ " + std::to_string(dmg) + " å∏è≠ÇµÇΩÅB", 1);
            phase = BattleScene::Phase::EnemyTurn;
        }
    }

    void ExecuteEnemyTurn(std::vector<BattleScene::Enemy>& enemies,
                          CharcterScene::CharcterDate& player,
                          PushLog pushLog)
    {
        std::vector<BattleScene::Enemy*> alive;
        for (auto& e : enemies) if (e.hp > 0) alive.push_back(&e);
        if (alive.empty()) return;
        // choose random alive attacker: Dice::RollDie returns 1..sides, convert to 0-based index
        int idx = Dice::RollDie(static_cast<int>(alive.size())) - 1;
        BattleScene::Enemy* attacker = alive[idx];
        int hit = Dice::RollDie(20);
        int dmg = (hit >= 6) ? attacker->atk : 1;
        player.ApplyEnduranceLoss(dmg);
        if (pushLog) pushLog(attacker->name + " Ç™çUåÇÇµ " + std::to_string(dmg) + " ÇÃëœãvóÕÇíDÇ¡ÇΩÅB", 1);
    }

    void UpdateTurn(std::vector<BattleScene::Enemy>& enemies,
                    CharcterScene::CharcterDate& player,
                    BattleScene::Phase& phase,
                    PushLog pushLog)
    {
        if (phase == BattleScene::Phase::EnemyTurn) {
            ExecuteEnemyTurn(enemies, player, pushLog);
            phase = BattleScene::Phase::PlayerTurn;
        }
    }

    bool CheckVictory(const std::vector<BattleScene::Enemy>& enemies)
    {
        for (const auto& e : enemies) if (e.hp > 0) return false;
        return true;
    }

    bool CheckDefeat(const CharcterScene::CharcterDate& player)
    {
        return player.endurance <= 0;
    }

}
