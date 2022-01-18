#include "post_processing.h"
#include "../../core_engine.h"
#include "../../util/mesh_factory.h"

namespace apex {
PostProcessing::PostProcessing()
  : m_render_scale(Vector2::One())
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

void PostProcessing::Render(Camera *cam, Framebuffer *fbo)
{
  CoreEngine::GetInstance()->DepthMask(false);
  CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::DEPTH_TEST);
  CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());

  for (auto &&it : m_filters) {
    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    it.filter->Begin(cam, fbo);

    m_quad->Render();

    it.filter->End(cam, fbo);
  }

  CoreEngine::GetInstance()->DepthMask(true);
  CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);
}

} // namespace apex
