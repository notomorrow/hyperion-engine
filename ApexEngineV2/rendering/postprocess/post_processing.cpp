#include "post_processing.h"
#include "../../core_engine.h"
#include "../../gl_util.h"
#include "../../util/mesh_factory.h"

namespace apex {
PostProcessing::PostProcessing()
    : m_render_scale(Vector2::One()),
      m_chained_textures_initialized(false),
      m_blit_framebuffer(nullptr)
{
    m_quad = MeshFactory::CreateQuad();
}

PostProcessing::~PostProcessing()
{
    if (m_blit_framebuffer != nullptr) {
        delete m_blit_framebuffer;
    }
}

void PostProcessing::RemoveFilter(const std::string &tag)
{
    const auto it = std::find_if(m_filters.begin(), m_filters.end(), [&tag](const Filter &filter) {
        return filter.tag == tag;
    });

    if (it != m_filters.end()) {
        m_filters.erase(it);
    }
}

void PostProcessing::Render(Camera *cam, Framebuffer2D *fbo)
{
    if (!m_chained_textures_initialized) {
        CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, fbo->GetId());

        for (int i = 0; i < m_chained_textures.size(); i++) {
            Framebuffer::FramebufferAttachment attachment = (Framebuffer::FramebufferAttachment)i;

            m_chained_textures[attachment] = Framebuffer2D::MakeTexture(
                attachment,
                fbo->GetWidth(),
                fbo->GetHeight(),
                nullptr
                // (unsigned char*)malloc(
                //     fbo->GetWidth() * fbo->GetHeight() * (Texture::NumComponents(
                //         Framebuffer::default_texture_attributes[attachment].format
                //     ) * sizeof(float))
                // )
            );

            // m_chained_textures[i]->Use(); // upload initial fbo data
        }

        CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        m_chained_textures_initialized = true;
    }

    if (m_blit_framebuffer == nullptr) {
        m_blit_framebuffer = new Framebuffer2D(
            fbo->GetWidth(),
            fbo->GetHeight(),
            true,
            true,
            true,
            true,
            true
        );
    }

    CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, fbo->GetId());
        for (int i = 0; i < m_chained_textures.size(); i++) {
            Framebuffer::FramebufferAttachment attachment = (Framebuffer::FramebufferAttachment)i;

            fbo->Store(attachment, m_chained_textures[attachment]);
        }
    CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    CoreEngine::GetInstance()->DepthMask(false);
    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::DEPTH_TEST);
    CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());

    m_blit_framebuffer->Use();

    for (auto &&it : m_filters) {
        CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

        it.filter->Begin(cam, m_chained_textures);

        m_quad->Render();

        it.filter->End(cam, m_blit_framebuffer, m_chained_textures); // store
    }
    
    m_blit_framebuffer->End();

    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);

    

    CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, m_blit_framebuffer->GetId());
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    CoreEngine::GetInstance()->BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);

    glBlitFramebuffer(
        0, 0, m_blit_framebuffer->GetWidth(), m_blit_framebuffer->GetHeight(),
        0, 0, fbo->GetWidth(), fbo->GetHeight(),
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

}

} // namespace apex
