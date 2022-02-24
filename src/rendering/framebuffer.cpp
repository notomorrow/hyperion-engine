#include "framebuffer.h"
#include "material.h"
#include "../opengl.h"
#include "../core_engine.h"

#include <iostream>

namespace hyperion {

decltype(Framebuffer::default_texture_attributes)
Framebuffer::default_texture_attributes = {
    Framebuffer::FramebufferTextureAttributes( // color
        MaterialTextureKey::MATERIAL_TEXTURE_COLOR_MAP,
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        true
    ),
    Framebuffer::FramebufferTextureAttributes( // normals
        MaterialTextureKey::MATERIAL_TEXTURE_NORMAL_MAP,
        CoreEngine::GLEnums::RGB,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // positions
        MaterialTextureKey::MATERIAL_TEXTURE_POSITION_MAP,
        CoreEngine::GLEnums::RGB,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // userdata
        MaterialTextureKey::MATERIAL_TEXTURE_DATA_MAP,
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA8,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // ssao / gi
        MaterialTextureKey::MATERIAL_TEXTURE_SSAO_MAP,
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA8,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        true
    ),
    Framebuffer::FramebufferTextureAttributes( // tangents
        MaterialTextureKey::MATERIAL_TEXTURE_TANGENT_MAP,
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // bitangents
        MaterialTextureKey::MATERIAL_TEXTURE_BITANGENT_MAP,
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),

    // *** depth must remain last
    Framebuffer::FramebufferTextureAttributes( // depth
        MaterialTextureKey::MATERIAL_TEXTURE_DEPTH_MAP,
        CoreEngine::GLEnums::DEPTH_COMPONENT,
        CoreEngine::GLEnums::DEPTH_COMPONENT32,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    )
};

Framebuffer::Framebuffer(int width, int height)
    : width(width),
      height(height)
{
    is_uploaded = false;
    is_created = false;
}

Framebuffer::~Framebuffer()
{
    if (is_created) {
        glDeleteFramebuffers(1, &id);
    }

    is_uploaded = false;
    is_created = false;
}

void Framebuffer::End()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace hyperion
