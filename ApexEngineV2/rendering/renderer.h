#ifndef RENDERER_H
#define RENDERER_H

#include <vector>

#include "../core_engine.h"
#include "../entity.h"
#include "material.h"
#include "camera/camera.h"
#include "framebuffer.h"
#include "postprocess/post_processing.h"

namespace apex {
struct BucketItem {
    Renderable *renderable;
    Material *material;
    Transform transform;
};

using Bucket_t = std::vector<BucketItem>;

class Shader;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void ClearRenderables();
    void FindRenderables(Entity *top);
    void RenderBucket(Camera *cam, Bucket_t &bucket, Shader *override_shader = nullptr);
    void RenderAll(Camera *cam, Framebuffer *fbo = nullptr);
    void RenderPost(Camera *cam, Framebuffer *fbo);

    inline PostProcessing *GetPostProcessing() { return m_post_processing; }
    inline const PostProcessing *GetPostProcessing() const { return m_post_processing; }

    Bucket_t sky_bucket;
    Bucket_t opaque_bucket;
    Bucket_t transparent_bucket;
    Bucket_t particle_bucket;

private:
    PostProcessing *m_post_processing;
};
} // namespace apex

#endif
