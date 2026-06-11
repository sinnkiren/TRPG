#include "StoryEditorScene.h"
#include "StoryPlayer.h"
#include "SceneManager.h"
#include "Logging.h"

struct StoryEditorScene::Impl {
    StoryPlayer player;
};

StoryEditorScene::StoryEditorScene()
    : pimpl(new Impl())
{
}

StoryEditorScene::~StoryEditorScene()
{
    delete pimpl;
    pimpl = nullptr;
}

void StoryEditorScene::Initialize()
{
    // Initialize underlying StoryPlayer so it has default data loaded
    try {
        pimpl->player.Initialize();
        ::Log::Log(::Log::Level::Info, "StoryEditorScene: StoryPlayer initialized for editor");
    }
    catch (...) {
        ::Log::Log(::Log::Level::Warning, "StoryEditorScene: failed to initialize StoryPlayer");
    }
}

void StoryEditorScene::Update()
{
    // Forward update to player so it stays in a consistent state when needed
    pimpl->player.Update();
}

void StoryEditorScene::Render()
{
    // Render underlying player visuals if any (not strictly necessary for editor but keep parity)
    pimpl->player.Render();
}

#ifndef NDEBUG
void StoryEditorScene::RenderUI()
{
    // Only allow editor UI in Dev mode
    if (!g_SceneManager.IsDevMode()) return;
    // Delegate to StoryPlayer's editor UI
    pimpl->player.RenderEditorUI();
}

void StoryEditorScene::RenderDevPanelContents()
{
    pimpl->player.RenderDevPanelContents();
}
#endif
