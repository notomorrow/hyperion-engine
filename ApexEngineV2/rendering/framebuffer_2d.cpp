#include "./framebuffer_2d.h"
#include "../opengl.h"

#include <iostream>

namespace apex {

Framebuffer2D::Framebuffer2D(int width, int height)
    : Framebuffer(width, height)
{
    color_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    color_texture->SetInternalFormat(GL_RGB16);
    color_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    color_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    normal_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    normal_texture->SetInternalFormat(GL_RGBA32F);
    normal_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    normal_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    position_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    position_texture->SetInternalFormat(GL_RGBA32F);
    position_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    position_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    depth_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    depth_texture->SetInternalFormat(GL_DEPTH_COMPONENT32F);
    depth_texture->SetFormat(GL_DEPTH_COMPONENT);
    depth_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    depth_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
}

Framebuffer2D::~Framebuffer2D()
{
}

const std::shared_ptr<Texture> Framebuffer2D::GetColorTexture() const { return color_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetNormalTexture() const { return normal_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetPositionTexture() const { return position_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetDepthTexture() const { return depth_texture; }

void Framebuffer2D::Use()
{
    if (!is_created) {
        glGenFramebuffers(1, &id);
        is_created = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, id);
    glViewport(0, 0, width, height);

    if (!is_uploaded) {
        color_texture->Use();
        glFramebufferTexture(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, color_texture->GetId(), 0);
        color_texture->End();

        normal_texture->Use();
        glFramebufferTexture(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT1, normal_texture->GetId(), 0);
        normal_texture->End();

        position_texture->Use();
        glFramebufferTexture(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT2, position_texture->GetId(), 0);
        position_texture->End();

        depth_texture->Use();
        glFramebufferTexture(GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT, depth_texture->GetId(), 0);
        depth_texture->End();

        const unsigned int draw_buffers[] = {
            GL_COLOR_ATTACHMENT0, // color map
            GL_COLOR_ATTACHMENT1, // normal map
            GL_COLOR_ATTACHMENT2  // position map
        };
        glDrawBuffers(3, draw_buffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Could not create framebuffer");
        }

        is_uploaded = true;
    }

}

void Framebuffer2D::StoreColor()
{
    color_texture->Use();

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    color_texture->End();
}

void Framebuffer2D::StoreDepth()
{
    depth_texture->Use();

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    depth_texture->End();
}

} // namespace apex
