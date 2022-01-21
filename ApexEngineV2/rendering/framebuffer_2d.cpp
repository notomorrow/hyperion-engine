#include "./framebuffer_2d.h"
#include "../gl_util.h"

namespace apex {

Framebuffer2D::Framebuffer2D(
    int width,
    int height,
    bool has_color_texture,
    bool has_depth_texture,
    bool has_normal_texture,
    bool has_position_texture,
    bool has_data_texture,
    bool has_ao_texture
)
    : Framebuffer(width, height),
      m_has_color_texture(has_color_texture),
      m_has_depth_texture(has_depth_texture),
      m_has_normal_texture(has_normal_texture),
      m_has_position_texture(has_position_texture),
      m_has_data_texture(has_data_texture),
      m_has_ao_texture(has_ao_texture)
{
    if (m_has_color_texture) {
        m_color_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
        m_color_texture->SetInternalFormat(GL_RGB32F);
        m_color_texture->SetFormat(GL_RGB);
        m_color_texture->SetFilter(GL_NEAREST, GL_NEAREST);
        m_color_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    if (m_has_normal_texture) {
        m_normal_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
        m_normal_texture->SetInternalFormat(GL_RGBA32F);
        m_normal_texture->SetFormat(GL_RGBA);
        m_normal_texture->SetFilter(GL_NEAREST, GL_NEAREST);
        m_normal_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    if (m_has_position_texture) {
        m_position_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
        m_position_texture->SetInternalFormat(GL_RGBA32F);
        m_position_texture->SetFormat(GL_RGBA);
        m_position_texture->SetFilter(GL_NEAREST, GL_NEAREST);
        m_position_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    if (m_has_depth_texture) {
        m_depth_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
        m_depth_texture->SetInternalFormat(GL_DEPTH_COMPONENT32F);
        m_depth_texture->SetFormat(GL_DEPTH_COMPONENT);
        m_depth_texture->SetFilter(GL_NEAREST, GL_NEAREST);
        m_depth_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    if (m_has_data_texture) {
        m_data_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
        m_data_texture->SetInternalFormat(GL_RGBA8);
        m_data_texture->SetFormat(GL_RGBA);
        m_data_texture->SetFilter(GL_NEAREST, GL_NEAREST);
        m_data_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    if (m_has_ao_texture) {
        m_ao_texture = std::make_shared<Texture2D>(width, height, (unsigned char*)nullptr);
        m_ao_texture->SetInternalFormat(GL_RGBA8);
        m_ao_texture->SetFormat(GL_RGBA);
        m_ao_texture->SetFilter(GL_NEAREST, GL_NEAREST);
        m_ao_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }
}

Framebuffer2D::~Framebuffer2D()
{
    // deleted in parent destructor
}

const std::shared_ptr<Texture> Framebuffer2D::GetColorTexture() const { return m_color_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetNormalTexture() const { return m_normal_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetPositionTexture() const { return m_position_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetDepthTexture() const { return m_depth_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetDataTexture() const { return m_data_texture; }
const std::shared_ptr<Texture> Framebuffer2D::GetAoTexture() const { return m_ao_texture; }

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
        unsigned int draw_buffers[5] = { GL_NONE };
        int draw_buffer_index = 0;

        if (m_has_color_texture) {
            m_color_texture->Begin();
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color_texture->GetId(), 0);
            CatchGLErrors("Failed to attach color attachment 0 to framebuffer.", false);
            m_color_texture->End();

            draw_buffers[draw_buffer_index++] = GL_COLOR_ATTACHMENT0;
        }

        if (m_has_normal_texture) {
            m_normal_texture->Begin();
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_normal_texture->GetId(), 0);
            CatchGLErrors("Failed to attach color attachment 1 to framebuffer.", false);
            m_normal_texture->End();

            draw_buffers[draw_buffer_index++] = GL_COLOR_ATTACHMENT1;
        }

        if (m_has_position_texture) {
            m_position_texture->Begin();
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_position_texture->GetId(), 0);
            CatchGLErrors("Failed to attach color attachment 2 to framebuffer.", false);
            m_position_texture->End();

            draw_buffers[draw_buffer_index++] = GL_COLOR_ATTACHMENT2;
        }

        if (m_has_data_texture) {
            m_data_texture->Begin();
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_data_texture->GetId(), 0);
            CatchGLErrors("Failed to attach color attachment 3 to framebuffer.", false);
            m_data_texture->End();

            draw_buffers[draw_buffer_index++] = GL_COLOR_ATTACHMENT3;
        }

        if (m_has_ao_texture) {
            m_ao_texture->Use();
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, m_ao_texture->GetId(), 0);
            CatchGLErrors("Failed to attach color attachment 4 to framebuffer.", false);
            m_ao_texture->End();

            draw_buffers[draw_buffer_index++] = GL_COLOR_ATTACHMENT4;
        }

        if (m_has_depth_texture) {
            m_depth_texture->Begin();
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth_texture->GetId(), 0);
            CatchGLErrors("Failed to attach depth texture to framebuffer.", false);
            m_depth_texture->End();
        }

        glDrawBuffers(draw_buffer_index, draw_buffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Could not create framebuffer");
        }

        is_uploaded = true;
    }

}

// TODO convert to index format
// TODO: dont copy textures, just modify directly
void Framebuffer2D::Store(const std::shared_ptr<Texture> &texture, int index)
{
    texture->Use();

    glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    glReadBuffer(0);

    texture->End();
}

} // namespace apex
