#pragma once
#include <string>
#include <vector>
#include "system/imgui/imgui.h"

namespace Log
{
    enum class Level { Error = 0, Warning = 1, Info = 2, Debug = 3 };

    struct Entry {
        double time; // ImGui::GetTime()
        Level level;
        std::string message;
    };

    // Initialize logger. Optionally provide a file path to append logs to.
    void Initialize(int maxLines = 1000, const std::string& filePath = "");
    void Shutdown();

    void SetDevMode(bool dev);
    bool IsDevMode();

    void SetMaxLines(int maxLines);

    void Log(Level lvl, const std::string& msg);
    void Logf(Level lvl, const char* fmt, ...);

    // Return a copy of current entries (thread-safe)
    std::vector<Entry> GetEntries();

    // Render simple ImGui window to show logs
    void RenderImGui(const char* title = "Log", bool* p_open = nullptr);

    // Install handlers to capture terminate
    void InstallGlobalHandlers();
}
