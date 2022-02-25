#ifndef FRAMEBUFFER_CUBE_H
#define FRAMEBUFFER_CUBE_H

#include "./framebuffer.h"
#include "./cubemap.h"

namespace hyperion {

class FramebufferCube : public Framebuffer {
public:
    FramebufferCube(int width, int height);
    virtual ~FramebufferCube();

    virtual void Store(FramebufferAttachment attachment, std::shared_ptr<Texture> &texture) override;

    virtual void Use() override;
    virtual void End() override;
};

} // namespace hyperion


#endif
