#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "texture_2D.h"

#include <memory>

namespace apex {

class Framebuffer {
public:
    Framebuffer(int width, int height);
    virtual ~Framebuffer();

    inline unsigned int GetId() const { return id; }
    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }

    virtual const std::shared_ptr<Texture> GetColorTexture() const = 0;
    virtual const std::shared_ptr<Texture> GetNormalTexture() const = 0;
    virtual const std::shared_ptr<Texture> GetPositionTexture() const = 0;
    virtual const std::shared_ptr<Texture> GetDepthTexture() const = 0;
    virtual const std::shared_ptr<Texture> GetDataTexture() const = 0;

    virtual void Store(const std::shared_ptr<Texture> &texture, int index) = 0;

    virtual void Use() = 0;
    void End();

protected:
    unsigned int id;
    int width, height;
    bool is_created, is_uploaded;
};

} // namespace apex

#endif
