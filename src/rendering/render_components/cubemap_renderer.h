#ifndef HYPERION_V2_CUBEMAP_RENDERER_H
#define HYPERION_V2_CUBEMAP_RENDERER_H

#include "../base.h"
#include "../post_fx.h"
#include "../renderer.h"
#include "../Light.h"
#include "../render_component.h"
#include "../buffers.h"

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
    CubemapRenderer(
        const Extent2D &cubemap_dimensions = Extent2D { 512, 512 },
        const Vector3 &origin              = Vector3::Zero()
    );
    CubemapRenderer(const CubemapRenderer &other) = delete;
    CubemapRenderer &operator=(const CubemapRenderer &other) = delete;
    ~CubemapRenderer();

    const Vector3 &GetOrigin() const      { return m_origin; }
    void SetOrigin(const Vector3 &origin) { m_origin = origin; }

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    static const std::array<std::pair<Vector3, Vector3>, 6> cubemap_directions;

    renderer::ImageView *GetCubemapImageView(UInt frame_index) const
    {
        return m_framebuffers[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView();
    }

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
    Vector3                                            m_origin;
    Ref<Scene>                                         m_scene;
    std::array<Ref<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Ref<Shader>                                        m_shader;
    Ref<RenderPass>                                    m_render_pass;
    Ref<GraphicsPipeline>                              m_pipeline;
    std::vector<std::unique_ptr<Attachment>>           m_attachments;
    Ref<Texture>                                       m_cubemap;
    UniformBuffer                                      m_uniform_buffer;
};


} // namespace hyperion::v2

#endif
