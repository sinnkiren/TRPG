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
        lastRoll = Dice::RollDie(100);
        bool success = (lastRoll <= player.pow * 5);
        if (success) {
            int dmg = 4 + Dice::RollDie(3); // 4 + d3
            target.hp = std::max(0, target.hp - dmg);
            if (pushLog) pushLog(player.name + " ‚Í " + target.name + " ‚É " + std::to_string(dmg) + " ‚Ìƒ_ƒ[ƒW‚ğ—^‚¦‚½B", 1);
            if (target.hp == 0 && pushLog) pushLog(target.name + " ‚ğ“|‚µ‚½I", 1);
        }
        else {
            int dmg = target.fearDamage;
            player.ApplyEnduranceLoss(dmg);
            if (pushLog) pushLog(player.name + " ‚ÌUŒ‚‚Í¸”s‚µ‚½B‘Ï‹v—Í‚ª " + std::to_string(dmg) + " Œ¸­‚µ‚½B", 1);
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
        BattleScene::Enemy* attacker = alive[Dice::RollDie((int)alive.size() - 1)];
        int hit = Dice::RollDie(20);
        int dmg = (hit >= 6) ? attacker->atk : 1;
        player.ApplyEnduranceLoss(dmg);
        if (pushLog) pushLog(attacker->name + " ‚ªUŒ‚‚µ " + std::to_string(dmg) + " ‚Ì‘Ï‹v—Í‚ğ’D‚Á‚½B", 1);
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
