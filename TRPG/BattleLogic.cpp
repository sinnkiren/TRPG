#include "BattleLogic.h"
#include "Dice.h"
#include <algorithm>
#include "Logging.h"

namespace BattleLogic {

    void PlayerAttack(BattleScene::Enemy& target,
                      CharacterScene::CharacterData& player,
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
            if (pushLog) pushLog(player.name + " は " + target.name + " に " + std::to_string(dmg) + " のダメージを与えた。", 1);
            if (target.hp == 0 && pushLog) pushLog(target.name + " を倒した！", 1);
        }
        else {
            int dmg = target.fearDamage;
            player.ApplyEnduranceLoss(dmg);
            if (pushLog) pushLog(player.name + " の攻撃は失敗した。耐久力が " + std::to_string(dmg) + " 減少した。", 1);
            phase = BattleScene::Phase::EnemyTurn;
        }
    }

    void ExecuteEnemyTurn(std::vector<BattleScene::Enemy>& enemies,
                          CharacterScene::CharacterData& player,
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
        if (pushLog) pushLog(attacker->name + " が攻撃し " + std::to_string(dmg) + " の耐久力を奪った。", 1);
    }

    void UpdateTurn(std::vector<BattleScene::Enemy>& enemies,
                    CharacterScene::CharacterData& player,
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

    bool CheckDefeat(const CharacterScene::CharacterData& player)
    {
        return player.endurance <= 0;
    }

}
