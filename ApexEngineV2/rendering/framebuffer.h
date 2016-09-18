#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "texture_2D.h"

#include <memory>

namespace apex {
class Framebuffer {
public:
    Framebuffer(int width, int height);
    ~Framebuffer();

    std::shared_ptr<Texture2D> GetColorTexture() const;
    std::shared_ptr<Texture2D> GetDepthTexture() const;

    unsigned int GetId() const;

    void Use();
    void End();

private:
    unsigned int id;
    int width, height;
    bool is_created, is_uploaded;

    std::shared_ptr<Texture2D> color_texture, depth_texture;
};
} // namespace apex

#endif