#ifndef FRAMEBUFFER_2D_H
#define FRAMEBUFFER_2D_H

#include "./framebuffer.h"
#include "./texture_2D.h"

#include <memory>

namespace apex {

class Framebuffer2D : public Framebuffer {
public:
    Framebuffer2D(
        int width,
        int height,
        bool has_color_texture = true,
        bool has_depth_texture = true,
        bool has_normal_texture = true,
        bool has_position_texture = true
    );
    virtual ~Framebuffer2D();

    virtual const std::shared_ptr<Texture> GetColorTexture() const override;
    virtual const std::shared_ptr<Texture> GetNormalTexture() const override;
    virtual const std::shared_ptr<Texture> GetPositionTexture() const override;
    virtual const std::shared_ptr<Texture> GetDepthTexture() const override;

    virtual void StoreColor() override;
    virtual void StoreDepth() override;

    virtual void Use() override;

private:
    std::shared_ptr<Texture2D> m_color_texture,
                               m_normal_texture,
                               m_depth_texture,
                               m_position_texture;
    bool m_has_color_texture,
         m_has_normal_texture,
         m_has_depth_texture,
         m_has_position_texture;
};

} // namespace apex

#endif
