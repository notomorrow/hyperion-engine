#include "./framebuffer_2d.h"
#include "../util.h"

namespace apex {

Framebuffer2D::Framebuffer2D(int width, int height)
    : Framebuffer(width, height)
{
    color_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    color_texture->SetInternalFormat(GL_RGBA8);
    color_texture->SetFormat(GL_RGBA);
    color_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    color_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    normal_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    normal_texture->SetInternalFormat(GL_RGBA8);
    color_texture->SetFormat(GL_RGBA);
    normal_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    normal_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    position_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    position_texture->SetInternalFormat(GL_RGBA8);
    color_texture->SetFormat(GL_RGBA);
    position_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    position_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    depth_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    depth_texture->SetInternalFormat(GL_DEPTH_COMPONENT16);
    depth_texture->SetFormat(GL_DEPTH_COMPONENT);
    depth_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    depth_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
}

Framebuffer2D::~Framebuffer2D()
{
    // deleted in parent destructor
}

const std::shared_ptr<Texture> Framebuffer2D::GetColorTexture() const { return color_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetNormalTexture() const { return normal_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetPositionTexture() const { return position_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetDepthTexture() const { return depth_texture; }

void Framebuffer2D::Use()
{
    if (!is_created) {
        glGenFramebuffers(1, &id);
        CatchGLErrors("Failed to generate framebuffer.");

        is_created = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, id);
    CatchGLErrors("Failed to bind framebuffer.", false);

    glViewport(0, 0, width, height);

    if (!is_uploaded) {
        color_texture->Use();
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture->GetId(), 0);
        CatchGLErrors("Failed to attach color attachment 0 to framebuffer.", false);
        color_texture->End();

        normal_texture->Use();
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_texture->GetId(), 0);
        CatchGLErrors("Failed to attach color attachment 1 to framebuffer.", false);
        normal_texture->End();

        position_texture->Use();
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, position_texture->GetId(), 0);
        CatchGLErrors("Failed to attach color attachment 2 to framebuffer.", false);
        position_texture->End();

        depth_texture->Use();
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture->GetId(), 0);
        CatchGLErrors("Failed to attach depth texture to framebuffer.", false);
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
