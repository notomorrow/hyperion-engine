#ifndef FRAMEBUFFER_2D_H
#define FRAMEBUFFER_2D_H

#include "./framebuffer.h"
#include "./texture_2D.h"

#include <memory>

namespace apex {

class Framebuffer2D : public Framebuffer {
public:
    Framebuffer2D(int width, int height);
    virtual ~Framebuffer2D();

    virtual const std::shared_ptr<Texture> GetColorTexture() const override;
    virtual const std::shared_ptr<Texture> GetNormalTexture() const override;
    virtual const std::shared_ptr<Texture> GetPositionTexture() const override;
    virtual const std::shared_ptr<Texture> GetDepthTexture() const override;

    virtual void StoreColor() override;
    virtual void StoreDepth() override;

    virtual void Use() override;

private:
    std::shared_ptr<Texture2D> color_texture, normal_texture, depth_texture, position_texture;
};

} // namespace apex

#endif
