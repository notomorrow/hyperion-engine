#ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include <map>
#include <cstdlib>

#include "../util.h"
#include "../core_engine.h"
#include "../scene/node.h"
#include "../hash_code.h"
#include "scene/spatial.h"
#include "math/bounding_box.h"
#include "render_window.h"
#include "material.h"
#include "camera/camera.h"
#include "framebuffer_2d.h"
#include "pipeline/render_queue.h"
#include "pipeline/deferred_pipeline.h"

namespace hyperion {

class Environment;

class Shader;

class Renderer {
public:
    Renderer(const RenderWindow &);
    Renderer(const Renderer &other) = delete;
    Renderer &operator=(const Renderer &other) = delete;
    ~Renderer();

    void Render(Camera *cam);

    void RenderBucket(Camera *cam, Bucket &bucket, Shader *override_shader = nullptr, bool enable_frustum_culling = true);
    void RenderAll(Camera *cam, Framebuffer2D *fbo = nullptr);

    inline const Framebuffer2D *GetFramebuffer() const { return m_fbo; }

    inline DeferredPipeline *GetDeferredPipeline() { return m_deferred_pipeline; }
    inline const DeferredPipeline *GetDeferredPipeline() const { return m_deferred_pipeline; }

    inline RenderWindow &GetRenderWindow() { return m_render_window; }
    inline const RenderWindow &GetRenderWindow() const { return m_render_window; }

    inline Environment *GetEnvironment() { return m_environment; }
    inline const Environment *GetEnvironment() const { return m_environment; }

    inline Bucket &GetBucket(Spatial::Bucket bucket) { return m_queue->GetBucket(bucket); }

    void ClearRenderables();

private:
    RenderQueue *m_queue;
    DeferredPipeline *m_deferred_pipeline;

    Framebuffer2D *m_fbo;
    RenderWindow m_render_window;
    int m_octree_callback_id;

    Environment *m_environment;

    std::map<Node*, HashCode::Value_t> m_hash_cache;
    std::map<HashCode::Value_t, Spatial::Bucket> m_hash_to_bucket;

    void SetRendererDefaults();
};
} // namespace hyperion

#endif
