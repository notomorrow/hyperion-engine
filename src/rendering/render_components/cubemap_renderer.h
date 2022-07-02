#ifndef HYPERION_V2_CUBEMAP_RENDERER_H
#define HYPERION_V2_CUBEMAP_RENDERER_H

#include "../base.h"
#include "../post_fx.h"
#include "../renderer.h"
#include "../Light.h"
#include "../render_component.h"

#include <rendering/backend/renderer_frame.h>

#include <math/bounding_box.h>
#include <scene/scene.h>
#include <camera/camera.h>
#include <types.h>

#include <array>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Extent2D;

class CubemapRenderer : public EngineComponentBase<STUB_CLASS(CubemapRenderer)>, public RenderComponent<CubemapRenderer> {
public:
    CubemapRenderer(const Extent2D &cubemap_dimensions = Extent2D { 512, 512 });
    CubemapRenderer(const CubemapRenderer &other) = delete;
    CubemapRenderer &operator=(const CubemapRenderer &other) = delete;
    ~CubemapRenderer();

    Ref<Texture> &GetCubemap()             { return m_cubemap; }
    const Ref<Texture> &GetCubemap() const { return m_cubemap; }

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

    // void RenderVoxels(Engine *engine, Frame *frame);

private:
    static const std::array<std::pair<Vector3, Vector3>, 6> cubemap_directions;

    void CreateImagesAndBuffers(Engine *engine);
    void CreateGraphicsPipelines(Engine *engine);
    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffers(Engine *engine);

    virtual void OnEntityAdded(Ref<Spatial> &spatial);
    virtual void OnEntityRemoved(Ref<Spatial> &spatial);
    virtual void OnEntityRenderableAttributesChanged(Ref<Spatial> &spatial);
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Extent2D                                           m_cubemap_dimensions;
    std::array<Ref<Scene>, 6>                          m_scenes;
    std::array<Ref<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Ref<Shader>                                        m_shader;
    Ref<RenderPass>                                    m_render_pass;
    std::array<Ref<GraphicsPipeline>, 6>               m_pipelines;
    std::vector<std::unique_ptr<Attachment>>           m_attachments;


    Ref<Texture>                                       m_cubemap;
};


} // namespace hyperion::v2

#endif
