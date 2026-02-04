#pragma once

class IScene {
public:
    virtual ~IScene() {}
    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Render() = 0;
#ifndef NDEBUG
    // Optional UI/rendering hook (ImGui etc). Default no-op so existing scenes don't need to implement it.
    virtual void RenderUI() {}

    // Optional per-scene embedded Dev Panel contents (called from centralized Dev Panel).
    // Default no-op so scenes that don't provide Dev info don't need to implement it.
    virtual void RenderDevPanelContents() {}
#else
    // In Release builds, provide a trivial inline RenderUI to avoid virtual call overhead / missing symbol.
    inline void RenderUI() {}
#endif
};