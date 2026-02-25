#pragma once
#include <string>

class AssetManager {
public:
    // Returns absolute or best-effort path to the assets root directory.
    static std::string GetAssetRoot();
    static std::string GetStoryPath(const std::string& name);
    static std::string GetTexturePath(const std::string& name);
};
