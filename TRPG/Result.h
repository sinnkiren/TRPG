#pragma once
#include "IScene.h"

class Result : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;
};