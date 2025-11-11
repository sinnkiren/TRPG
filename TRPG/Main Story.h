#pragma once

#include "BattleScene.h"
#include "IScene.h"
#include <memory>

class MainStory : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;

private:
    std::unique_ptr<BattleScene> battleScene; // Holds the battle scene for gameplay-only main
};