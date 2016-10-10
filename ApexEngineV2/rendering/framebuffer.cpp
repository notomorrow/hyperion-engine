#include "framebuffer.h"
#include "../opengl.h"

namespace apex {

Framebuffer::Framebuffer(int width, int height)
    : width(width), 
      height(height)
{
    is_uploaded = false;
    is_created = false;

    color_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    color_texture->SetInternalFormat(GL_RGB8);
    color_texture->SetFilter(GL_NEAREST, GL_NEAREST);

    depth_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
    depth_texture->SetInternalFormat(GL_DEPTH_COMPONENT24);
    depth_texture->SetFormat(GL_DEPTH_COMPONENT);
    depth_texture->SetFilter(GL_NEAREST, GL_NEAREST);
}

Framebuffer::~Framebuffer()
{
    if (is_created) {
        glDeleteFramebuffers(1, &id);
    }
    is_uploaded = false;
    is_created = false;
}

void Framebuffer::Use()
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

        depth_texture->Use();
        glFramebufferTexture(GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT, depth_texture->GetId(), 0);
        depth_texture->End();

        unsigned int db = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &db);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Could not create framebuffer");
        }

        is_uploaded = true;
    }

}

void Framebuffer::End()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace apex