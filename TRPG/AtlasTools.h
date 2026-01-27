#pragma once
#include "system/imgui/imgui.h"
#include <string>

namespace AtlasTools {

    struct Region {
        ImVec2 uv0;
        ImVec2 uv1;
    };

    struct AtlasMap {
        Region statusBox;
        Region dialogBox;
        Region redBar;
        Region blueBar;
        Region greenBar;
        Region buttonBox;
        bool valid = false;
    };

    AtlasMap AnalyzeAtlas(const std::string& relativePath, int alphaThreshold = 8);
}
