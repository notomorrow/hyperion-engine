#ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include <map>
#include <cstdlib>

#include "../util.h"
#include "../core_engine.h"
#include "../scene/node.h"
#include "../hash_code.h"
#include "../scene/scene_manager.h"
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

    void Render(Camera *cam, Octree::VisibilityState::CameraType camera_type);
    void RenderBucket(Camera *cam, Spatial::Bucket spatial_bucket, Octree::VisibilityState::CameraType camera_type, Shader *override_shader = nullptr);
    void RenderAll(Camera *cam, Octree::VisibilityState::CameraType camera_type, Framebuffer2D *fbo = nullptr);

    inline const Framebuffer2D *GetFramebuffer() const { return m_fbo; }

    inline DeferredPipeline *GetDeferredPipeline() { return m_deferred_pipeline; }
    inline const DeferredPipeline *GetDeferredPipeline() const { return m_deferred_pipeline; }

    inline RenderWindow &GetRenderWindow() { return m_render_window; }
    inline const RenderWindow &GetRenderWindow() const { return m_render_window; }

    inline Environment *GetEnvironment() { return m_environment; }
    inline const Environment *GetEnvironment() const { return m_environment; }

    inline Bucket &GetBucket(Spatial::Bucket bucket)
        { return m_all_items.GetBucket(bucket); }

private:
    RenderQueue m_all_items;
    DeferredPipeline *m_deferred_pipeline;

    Framebuffer2D *m_fbo;
    RenderWindow m_render_window;
    int m_octree_callback_id;

    Environment *m_environment;

    void SetRendererDefaults();
};
} // namespace hyperion

#endif
