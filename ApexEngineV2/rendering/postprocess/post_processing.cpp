#include "post_processing.h"
#include "../../core_engine.h"
#include "../../gl_util.h"
#include "../../util/mesh_factory.h"

namespace apex {
PostProcessing::PostProcessing()
    : m_render_scale(Vector2::One()),
      m_buffers({ nullptr }),
      m_buffers_created(false)
{
    m_quad = MeshFactory::CreateQuad();
}

PostProcessing::~PostProcessing()
{
    if (m_buffers_created) {
        for (int i = 0; i < m_buffers.size(); i++) {
            delete m_buffers[i];
        }
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
    if (!m_buffers_created) {
        for (int i = 0; i < m_buffers.size(); i++) {
            m_buffers[i] = new Framebuffer2D(
                fbo->GetWidth(),
                fbo->GetHeight(),
                true,
                true,
                true,
                true,
                true,
                true
            );

            m_buffers[i]->Use(); // TEMP just for now until a Prepare function is added
            m_buffers[i]->End();
        }
        
        m_buffers_created = true;
    }

    CoreEngine::GetInstance()->DepthMask(false);
    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::DEPTH_TEST);
    CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());

    int counter = 0;

    Framebuffer2D *read_buffer = fbo;
    Framebuffer2D *draw_buffer = m_buffers[0];

    for (auto &&it : m_filters) {
        draw_buffer->Use();
        CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT | CoreEngine::GLEnums::STENCIL_BUFFER_BIT);
        CatchGLErrors("Failed to use blit framebuffer");

        it.filter->Begin(cam, read_buffer);
        CatchGLErrors("Failed to begin filter");

        m_quad->Render();
        CatchGLErrors("Failed to render mesh");

        it.filter->End(cam, read_buffer);
        CatchGLErrors("Failed to end filter");


        draw_buffer->End();

        // CatchGLErrors("Failed to end blit framebuffer");
        // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, read_buffer->GetId());
        // CatchGLErrors("Failed to bind draw framebuffer");
        // glDrawBuffer(GL_COLOR_ATTACHMENT0);
        // CatchGLErrors("Failed to set draw buffer");

        // glBindFramebuffer(GL_READ_FRAMEBUFFER, draw_buffer->GetId());
        // CatchGLErrors("Failed to bind read framebuffer");
        // glReadBuffer(GL_COLOR_ATTACHMENT0);
        // CatchGLErrors("Failed to set readbuffer");
        // glBlitFramebuffer(0, 0, draw_buffer->GetWidth(), draw_buffer->GetHeight(), 0, 0, read_buffer->GetWidth(), read_buffer->GetHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        // CatchGLErrors("Failed to blit framebuffer");

        read_buffer = m_buffers[counter % 2];
        draw_buffer = m_buffers[(counter + 1) % 2];

        counter++;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    CatchGLErrors("Failed to bind draw framebuffer");
    glDrawBuffer(GL_BACK);
    CatchGLErrors("Failed to set draw buffer");

    glBindFramebuffer(GL_READ_FRAMEBUFFER, read_buffer->GetId());
    CatchGLErrors("Failed to bind read framebuffer");
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    CatchGLErrors("Failed to set readbuffer");
    glBlitFramebuffer(0, 0, read_buffer->GetWidth(), read_buffer->GetHeight(), 0, 0, read_buffer->GetWidth(), read_buffer->GetHeight(), GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    CatchGLErrors("Failed to blit framebuffer");

    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);
}

} // namespace apex
