#pragma once

class IRenderer2D {
public:
    virtual ~IRenderer2D() {}
    virtual void DrawText(const char* text, float x, float y, unsigned int color) = 0;
    virtual void DrawRectFilled(float x, float y, float w, float h, unsigned int color) = 0;
    virtual void DrawRect(float x, float y, float w, float h, unsigned int color) = 0;
};

// Engine should define this function to return the current 2D renderer instance.
extern IRenderer2D* GetRenderer2D();
