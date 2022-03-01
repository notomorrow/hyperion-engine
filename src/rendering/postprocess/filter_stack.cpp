#include "filter_stack.h"
#include "filters/default_filter.h"
#include "../../math/math_util.h"
#include "../../core_engine.h"
#include "../../gl_util.h"
#include "../../util/mesh_factory.h"

namespace hyperion {
FilterStack::FilterStack()
    : m_render_scale(Vector2::One()),
      m_save_last_to_fbo(true),
      m_gbuffer(nullptr)
{
    m_quad = MeshFactory::CreateQuad();
}

FilterStack::~FilterStack()
{
}

void FilterStack::RemoveFilter(const std::string &tag)
{
    const auto it = std::find_if(m_filters.begin(), m_filters.end(), [&tag](const Filter &filter) {
        return filter.tag == tag;
    });

    if (it != m_filters.end()) {
        m_filters.erase(it);
    }
}

void FilterStack::Render(Camera *cam, Framebuffer2D *read_fbo, Framebuffer2D *blit_fbo)
{
    AssertThrow(!m_filters.empty());
    AssertThrow(m_gbuffer != nullptr);

    blit_fbo->Use();

    // optimization: if counter == m_filters.size() - 1, end the fbo. then we can avoid copying textures one time,
    // as well as not having to blit the whole fbo to the backbuffer.
    int counter = 0;
    bool in_fbo = true;
    const int counter_max = m_filters.size() - 1;

    for (auto &&it : m_filters) {
        CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

        if (!m_save_last_to_fbo && counter == counter_max) {
            blit_fbo->End();
            in_fbo = false;
        }

        it.filter->Begin(cam, *m_gbuffer);

        m_quad->Render(nullptr, cam); // TODO

        it.filter->End(cam, blit_fbo, *m_gbuffer, in_fbo); // store

        counter++;
    }

    if (in_fbo) {
        blit_fbo->End();
        in_fbo = false;
    }
}

} // namespace hyperion
