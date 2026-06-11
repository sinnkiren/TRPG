#pragma once

#include "IScene.h"

class StoryEditorScene : public IScene {
public:
    StoryEditorScene();
    virtual ~StoryEditorScene();

    void Initialize() override;
    void Update() override;
    void Render() override;
#ifndef NDEBUG
    void RenderUI() override;
    void RenderDevPanelContents() override;
#else
    inline void RenderUI() override {}
    inline void RenderDevPanelContents() override {}
#endif

private:
    // Forward-declare to avoid including heavy headers here
    struct Impl;
    Impl* pimpl = nullptr;
};
