#pragma once
#include "IScene.h"

class ExploreScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Render() override;
    void RenderUI() override;
};
