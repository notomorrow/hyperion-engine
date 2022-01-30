#ifndef GI_RENDERER_H
#define GI_RENDERER_H

#include "gi_mapping.h"
#include "../camera/camera.h"

#include <array>
#include <memory>

namespace hyperion {

class Shader;
class ComputeShader;
class Renderer;

class GIRenderer {
public:
    GIRenderer();
    ~GIRenderer();

    void Render(Renderer *renderer, Camera *cam);

    inline GIMapping *GetGIMapping(int index) { return m_gi_map_renderers[index]; }
    inline const GIMapping *GetGIMapping(int index) const { return m_gi_map_renderers[index]; }
    inline size_t NumProbes() const { return m_gi_map_renderers.size(); }

private:
    std::shared_ptr<Shader> m_gi_shader;
    std::shared_ptr<ComputeShader> m_clear_shader;
    std::vector<GIMapping*> m_gi_map_renderers;
};

} // namespace apex

#endif