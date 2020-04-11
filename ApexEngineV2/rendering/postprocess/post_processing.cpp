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

void PostProcessing::Render(Camera *cam, Framebuffer *fbo)
{
  for (auto it : m_filters) {
    it.filter->Begin(cam, fbo);

    m_quad->Render();

    it.filter->End(cam, fbo);
  }
}

} // namespace apex
