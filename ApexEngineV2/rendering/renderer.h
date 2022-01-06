#ifndef RENDERER_H
#define RENDERER_H

#include <vector>

#include "../core_engine.h"
#include "../entity.h"
#include "render_window.h"
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
    Renderer(const RenderWindow &);
    ~Renderer();

    void Begin(Camera *cam, Entity *top);
    void Render(Camera *cam);
    void End(Camera * cam);

    void RenderBucket(Camera *cam, Bucket_t &bucket, Shader *override_shader = nullptr);
    void RenderAll(Camera *cam, Framebuffer *fbo = nullptr);
    void RenderPost(Camera *cam, Framebuffer *fbo);
 
    inline PostProcessing *GetPostProcessing() { return m_post_processing; }
    inline const PostProcessing *GetPostProcessing() const { return m_post_processing; }
    inline RenderWindow &GetRenderWindow() { return m_render_window; }
    inline const RenderWindow &GetRenderWindow() const { return m_render_window; }

    Bucket_t sky_bucket;
    Bucket_t opaque_bucket;
    Bucket_t transparent_bucket;
    Bucket_t particle_bucket;

private:
    PostProcessing *m_post_processing;
    Framebuffer *m_fbo;
    RenderWindow m_render_window;

    void ClearRenderables();
    void FindRenderables(Camera *cam, Entity *top);
};
} // namespace apex

#endif
