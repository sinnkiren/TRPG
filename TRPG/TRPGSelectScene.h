#pragma once
#include "IScene.h"

class TRPGSelectScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;
};