#include "./framebuffer_cube.h"
#include "../opengl.h"

#include <iostream>

namespace apex {

FramebufferCube::FramebufferCube(int width, int height)
    : Framebuffer(width, height)
{
    std::array<std::shared_ptr<Texture2D>, 6> color_textures = {
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr)
    };

    color_texture = std::make_shared<Cubemap>(color_textures);
    color_texture->SetInternalFormat(GL_RGB8);
    color_texture->SetFormat(GL_RGB);
    color_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    color_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    /*std::array<std::shared_ptr<Texture2D>, 6> depth_textures = {
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr),
        std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr)
    };

    depth_texture = std::make_shared<Cubemap>(depth_textures);
    depth_texture->SetInternalFormat(GL_RGB8);
    depth_texture->SetFormat(GL_RGB);
    depth_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    depth_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);*/
}

FramebufferCube::~FramebufferCube()
{
}

const std::shared_ptr<Texture> FramebufferCube::GetColorTexture() const { return color_texture; }
const std::shared_ptr<Texture> FramebufferCube::GetNormalTexture() const { return nullptr; }
const std::shared_ptr<Texture> FramebufferCube::GetPositionTexture() const { return nullptr; }
const std::shared_ptr<Texture> FramebufferCube::GetDepthTexture() const { return depth_texture; }

void FramebufferCube::Use()
{
    if (!is_created) {
        glGenFramebuffers(1, &id);
        is_created = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, id);
    glViewport(0, 0, width, height);

    if (!is_uploaded) {
        color_texture->Use();
        //glFramebufferTexture(GL_FRAMEBUFFER,
        //    GL_COLOR_ATTACHMENT0, color_texture->GetId(), 0);
        for (int i = 0; i < 6; i++) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, color_texture->GetId(), 0);
        }
        color_texture->End();

        /*depth_texture->Use();
        for (int i = 0; i < 6; i++) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT1, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, depth_texture->GetId(), 0);
        }
        // glFramebufferTexture(GL_FRAMEBUFFER,
        //     GL_DEPTH_ATTACHMENT, depth_texture->GetId(), 0);
        depth_texture->End();*/

        const unsigned int draw_buffers[] = {
            GL_COLOR_ATTACHMENT0, // color map
           // GL_COLOR_ATTACHMENT1
        };
        glDrawBuffers(1, draw_buffers);

        unsigned int status;
        if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Could not create FramebufferCube " << status << std::endl;
            throw std::runtime_error("Could not create FramebufferCube");
        }

        is_uploaded = true;
    }

}

void FramebufferCube::StoreColor()
{
    // not implemented
}

void FramebufferCube::StoreDepth()
{
    // not implemented
}

} // namespace apex
