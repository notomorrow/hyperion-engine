#ifndef RENDERER_H
#define RENDERER_H

#include <vector>

#include "../core_engine.h"
#include "../entity.h"
#include "camera/camera.h"

namespace apex {
class Renderer {
public:
    Renderer();

    void ClearRenderables();
    void FindRenderables(Entity *top);
    void RenderBucket(Camera *cam, std::vector<std::pair<Renderable*, Transform>> &bucket);
    void RenderAll(Camera *cam);

    std::vector<std::pair<Renderable*, Transform>> sky_bucket;
    std::vector<std::pair<Renderable*, Transform>> opaque_bucket;
    std::vector<std::pair<Renderable*, Transform>> transparent_bucket;
    std::vector<std::pair<Renderable*, Transform>> particle_bucket;
};
} // namespace apex

#endif