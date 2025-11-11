#pragma once

class IScene {
public:
    virtual ~IScene() {}
    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Render() = 0;
};