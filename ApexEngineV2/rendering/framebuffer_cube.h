#ifndef FRAMEBUFFER_CUBE_H
#define FRAMEBUFFER_CUBE_H

#include "./framebuffer.h"
#include "./cubemap.h"

namespace apex {

class FramebufferCube : public Framebuffer {
public:
    FramebufferCube(int width, int height);
    virtual ~FramebufferCube();

    virtual const std::shared_ptr<Texture> GetColorTexture() const override;
    virtual const std::shared_ptr<Texture> GetNormalTexture() const override;
    virtual const std::shared_ptr<Texture> GetPositionTexture() const override;
    virtual const std::shared_ptr<Texture> GetDepthTexture() const override;

    virtual void StoreColor() override;
    virtual void StoreDepth() override;

    virtual void Use() override;

private:
    std::shared_ptr<Cubemap> color_texture, depth_texture;
};

} // namespace apex


#endif
