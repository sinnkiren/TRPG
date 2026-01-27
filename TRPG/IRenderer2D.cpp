#include "IRenderer2D.h"
#include "system/imgui/imgui.h"

// Simple ImGui-backed 2D renderer that forwards Draw calls to ImGui's draw list.
// This is a lightweight implementation so the game can render basic text and rectangles
// without a dedicated graphics backend. For production replace with your engine's renderer.
namespace {
    struct ImGuiRenderer : IRenderer2D {
        void DrawText(const char* text, float x, float y, unsigned int color) override {
            ImU32 col = (ImU32)color;
            ImFont* font = ImGui::GetFont();
            float size = ImGui::GetFontSize();
            if (font)
                ImGui::GetForegroundDrawList()->AddText(font, size, ImVec2(x, y), col, text);
            else
                ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), col, text);
        }
        void DrawRectFilled(float x, float y, float w, float h, unsigned int color) override {
            ImU32 col = (ImU32)color;
            ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col);
        }
        void DrawRect(float x, float y, float w, float h, unsigned int color) override {
            ImU32 col = (ImU32)color;
            ImGui::GetForegroundDrawList()->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col);
        }
    } g_imguiRenderer;
}

IRenderer2D* GetRenderer2D() {
    return &g_imguiRenderer;
}
