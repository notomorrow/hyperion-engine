#ifndef GI_RENDERER_H
#define GI_RENDERER_H

#include "gi_mapping.h"
#include "../renderer.h"
#include "../camera/camera.h"

#include <array>
#include <memory>

namespace apex {

class Shader;

class GIRenderer {
public:
    GIRenderer(Camera *view_cam);
    ~GIRenderer() ;

    void Render(Renderer *renderer);

private:
    std::shared_ptr<Shader> m_gi_shader;
    std::array<GIMapping*, 6> m_gi_map_renderers;
};

} // namespace apex

#endif
