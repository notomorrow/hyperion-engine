#ifndef RENDER_WINDOW_H
#define RENDER_WINDOW_H

#include <string>

namespace apex {
struct RenderWindow {
    RenderWindow() 
        : width(0), 
          height(0), 
          title("Window") 
    {
    }

    RenderWindow(int width, int height, const std::string &title)
        : width(width), 
          height(height), 
          title(title)
    {
    }

    RenderWindow(const RenderWindow &other)
        : width(other.width), 
          height(other.height), 
          title(other.title)
    {
    }

    int width, height;
    std::string title;
};
} // namespace apex

#endif