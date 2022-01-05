#include "post_processing.h"
#include "../../util/mesh_factory.h"

namespace apex {
PostProcessing::PostProcessing()
{
  m_quad = MeshFactory::CreateQuad();
}

PostProcessing::~PostProcessing()
{
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

// void PostProcessing::UpdateFilterScaling(PostFilter *filter)
// {
//     if (filter->GetShader() == nullptr) {
//         throw std::runtime_error("No shader attached to PostFilter");
//     }

//     filter->GetShader()->SetUniform("u_scale", m_render_scale);
// }

// void PostProcessing::SetRenderScale(const Vector2 &render_scale)
// {
//     m_render_scale = render_scale;

//     for (auto &&it : m_filters) {
//         UpdateFilterScaling(it.filter.get());
//     }
// }

void PostProcessing::Render(Camera *cam, Framebuffer *fbo)
{
  glDepthMask(false);
  glDisable(GL_DEPTH_TEST);
  glViewport(0, 0, cam->GetWidth() * m_render_scale.x, cam->GetHeight() * m_render_scale.y);

  for (auto &&it : m_filters) {
    it.filter->Begin(cam, fbo);

    m_quad->Render();

    it.filter->End(cam, fbo);
  }

  glDepthMask(true);
  glEnable(GL_DEPTH_TEST);
}

} // namespace apex
