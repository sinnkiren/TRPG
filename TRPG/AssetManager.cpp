#include "AssetManager.h"
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <Windows.h>
#endif

using namespace std;
namespace fs = std::filesystem;

string AssetManager::GetAssetRoot()
{
    // Prefer EXE directory's parent, then current_path
#ifdef _WIN32
    char buf[MAX_PATH];
    if (GetModuleFileNameA(NULL, buf, MAX_PATH)) {
        fs::path exe(buf);
        fs::path parent = exe.parent_path();
        // assume project layout: <exe>/.. or <exe> has assets folder next to it
        fs::path candidate = parent;
        if (fs::exists(candidate / "assets")) return (candidate / "assets").string();
        // try parent/.. as fallback
        if (fs::exists(parent / ".." / "assets")) return (parent / ".." / "assets").string();
    }
#endif
    // fallback to current path/assets
    fs::path cur = fs::current_path();
    if (fs::exists(cur / "assets")) return (cur / "assets").string();
    return (cur / "assets").string();
}

string AssetManager::GetStoryPath(const std::string& name)
{
    fs::path root = GetAssetRoot();
    return (root / "story" / name).string();
}

string AssetManager::GetTexturePath(const std::string& name)
{
    fs::path root = GetAssetRoot();
    return (root / "texture" / name).string();
}
