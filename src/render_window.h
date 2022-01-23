#ifndef RENDER_WINDOW_H
#define RENDER_WINDOW_H

#include "math/math_util.h"
#include "math/vector2.h"

#include <string>

namespace hyperion {

struct RenderWindow {
    RenderWindow() 
        : width(0),
          height(0),
          xscale(1.0f),
          yscale(1.0f),
          title("Window") 
    {
    }

    RenderWindow(int width, int height, const std::string &title)
        : width(width), 
          height(height), 
          xscale(1.0f),
          yscale(1.0f),
          title(title)
    {
    }

    RenderWindow(const RenderWindow &other)
        : width(other.width), 
          height(other.height),
          xscale(other.xscale),
          yscale(other.yscale),
          title(other.title)
    {
    }

    inline int GetScaledWidth() const { return MathUtil::Floor(width * xscale); }
    inline int GetScaledHeight() const { return MathUtil::Floor(height * yscale); }

    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }
    const std::string &GetTitle() const { return title; }

    inline Vector2 GetScale() const { return Vector2(xscale, yscale); }
    inline void SetScale(const Vector2 &scale) { xscale = scale.x; yscale = scale.y; }

    int width, height;
    float xscale, yscale;
    std::string title;
};

} // namespace hyperion

#endif
