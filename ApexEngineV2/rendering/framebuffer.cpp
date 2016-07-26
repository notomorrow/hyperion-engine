#include "framebuffer.h"
#include "../core_engine.h"

namespace apex {
Framebuffer::Framebuffer(int width, int height)
    : width(width), height(height)
{
    is_uploaded = false;
    is_created = false;

    color_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)NULL);

    depth_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)NULL);
    depth_texture->SetInternalFormat(CoreEngine::DEPTH_COMPONENT16);
    depth_texture->SetFormat(CoreEngine::DEPTH_COMPONENT);
}

Framebuffer::~Framebuffer()
{
    if (is_created) {
        CoreEngine::GetInstance()->DeleteFramebuffers(1, &id);
    }
    is_uploaded = false;
    is_created = false;
}

unsigned int Framebuffer::GetId() const
{
    return id;
}

std::shared_ptr<Texture2D> Framebuffer::GetColorTexture() const
{
    return color_texture;
}

std::shared_ptr<Texture2D> Framebuffer::GetDepthTexture() const
{
    return depth_texture;
}

void Framebuffer::Use()
{
    if (!is_created) {
        CoreEngine::GetInstance()->GenFramebuffers(1, &id);
        is_created = true;
    }

    CoreEngine::GetInstance()->BindFramebuffer(CoreEngine::FRAMEBUFFER, id);
    CoreEngine::GetInstance()->Viewport(0, 0, width, height);

    if (!is_uploaded) {
        color_texture->Use();
        CoreEngine::GetInstance()->FramebufferTexture(CoreEngine::FRAMEBUFFER, 
            CoreEngine::COLOR_ATTACHMENT0, color_texture->GetId(), 0);
        color_texture->End();

        depth_texture->Use();
        CoreEngine::GetInstance()->FramebufferTexture(CoreEngine::FRAMEBUFFER,
            CoreEngine::DEPTH_ATTACHMENT, depth_texture->GetId(), 0);
        depth_texture->End();

        unsigned int db = CoreEngine::COLOR_ATTACHMENT0;
        CoreEngine::GetInstance()->DrawBuffers(1, &db);

        if (CoreEngine::GetInstance()->CheckFramebufferStatus(CoreEngine::FRAMEBUFFER) 
            != CoreEngine::FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Could not create framebuffer");
        }

        is_uploaded = true;
    }

}

void Framebuffer::End()
{
    CoreEngine::GetInstance()->BindFramebuffer(CoreEngine::FRAMEBUFFER, NULL);
}
}