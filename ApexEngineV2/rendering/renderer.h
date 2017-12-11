#ifndef RENDERER_H
#define RENDERER_H

#include <vector>

#include "../core_engine.h"
#include "../entity.h"
#include "material.h"
#include "camera/camera.h"

namespace apex {
struct BucketItem {
    Renderable *renderable;
    Material *material;
    Transform transform;
};

using Bucket_t = std::vector<BucketItem>;

class Renderer {
public:
    Renderer();

    void ClearRenderables();
    void FindRenderables(Entity *top);
    void RenderBucket(Camera *cam, Bucket_t &bucket);
    void RenderAll(Camera *cam);

    Bucket_t sky_bucket;
    Bucket_t opaque_bucket;
    Bucket_t transparent_bucket;
    Bucket_t particle_bucket;
};
} // namespace apex

#endif