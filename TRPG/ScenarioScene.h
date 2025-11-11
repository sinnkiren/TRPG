#pragma once
#include "IScene.h"

class ScenarioScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;
};