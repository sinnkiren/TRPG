#include "Logging.h"
#include <mutex>
#include <deque>
#include <cstdarg>
#include <cstdio>
#include <atomic>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <Windows.h>
#include <ctime>

namespace Log
{
    static std::mutex g_mutex;
    static std::deque<Entry> g_entries;
    static int g_maxLines = 1000;
    static std::atomic<bool> g_devMode{false};
    static std::ofstream g_file;
    static std::string g_filePath;

    void Initialize(int maxLines)
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_maxLines = maxLines;
        g_entries.clear();
        g_devMode = false;
    }

    void Initialize(int maxLines, const std::string& filePath)
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_maxLines = maxLines;
        g_entries.clear();
        g_devMode = false;
        g_filePath = filePath;
        if (!g_filePath.empty()) {
            g_file.open(g_filePath, std::ios::out | std::ios::app);
            if (g_file) {
                auto now = std::chrono::system_clock::now();
                auto tt = std::chrono::system_clock::to_time_t(now);
                std::tm tm{};
                localtime_s(&tm, &tt);
                g_file << "--- Log started: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ---\n";
                g_file.flush();
            }
        }
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_entries.clear();
        if (g_file) {
            g_file.flush();
            g_file.close();
        }
    }

    void SetDevMode(bool dev) { g_devMode = dev; }
    bool IsDevMode() { return g_devMode.load(); }

    void SetMaxLines(int maxLines)
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_maxLines = maxLines;
        while ((int)g_entries.size() > g_maxLines) g_entries.pop_front();
    }

    void PushEntry(Level lvl, const std::string& msg)
    {
        Entry e;
        e.time = ImGui::GetTime();
        e.level = lvl;
        e.message = msg;
        std::lock_guard<std::mutex> lk(g_mutex);
        g_entries.push_back(e);
        while ((int)g_entries.size() > g_maxLines) g_entries.pop_front();
    }

    void Log(Level lvl, const std::string& msg)
    {
        // In Play mode, ignore Debug level logs
        if (!IsDevMode() && lvl == Level::Debug) return;
        PushEntry(lvl, msg);
        // Also emit to debug output
        std::string out = "[" + std::to_string((int)lvl) + "] " + msg + "\n";
        OutputDebugStringA(out.c_str());
        if (g_file) {
            // timestamp
            auto now = std::chrono::system_clock::now();
            auto tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_s(&tm, &tt);
            std::lock_guard<std::mutex> lk(g_mutex);
            g_file << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " " << out;
            g_file.flush();
        }
    }

    void Logf(Level lvl, const char* fmt, ...)
    {
        char buf[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        Log(lvl, std::string(buf));
    }

    std::vector<Entry> GetEntries()
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        return std::vector<Entry>(g_entries.begin(), g_entries.end());
    }

    static const char* LevelToStr(Level l)
    {
        switch (l) {
        case Level::Error: return "ERR";
        case Level::Warning: return "WARN";
        case Level::Info: return "INFO";
        default: return "DBG";
        }
    }

    void RenderImGui(const char* title, bool* p_open)
    {
        if (!ImGui::Begin(title, p_open)) { ImGui::End(); return; }
        ImGui::BeginChild("log_scroller");
        auto entries = GetEntries();
        for (const auto& e : entries) {
            ImVec4 col = ImVec4(1,1,1,1);
            switch (e.level) {
            case Level::Error: col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break;
            case Level::Warning: col = ImVec4(1.0f, 0.8f, 0.3f, 1.0f); break;
            case Level::Info: col = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
            default: col = ImVec4(0.6f, 0.6f, 0.8f, 1.0f); break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::Text("[%.2f] %s: %s", e.time, LevelToStr(e.level), e.message.c_str());
            ImGui::PopStyleColor();
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::End();
    }

    // Basic terminate handler to try to capture unhandled exceptions
    static void TerminateHandler()
    {
        try {
            std::exception_ptr exptr = std::current_exception();
            if (exptr) std::rethrow_exception(exptr);
        }
        catch (const std::exception& ex) {
            Log(Level::Error, std::string("Unhandled exception: ") + ex.what());
        }
        catch (...) {
            Log(Level::Error, "Unhandled unknown exception");
        }
        // call default
        std::abort();
    }

    void InstallGlobalHandlers()
    {
        std::set_terminate(TerminateHandler);
    }
}
