#ifndef HYPERION_V2_ENV_PROBE_HPP
#define HYPERION_V2_ENV_PROBE_HPP

#include <core/Base.hpp>
#include <core/lib/Optional.hpp>
#include <math/BoundingBox.hpp>
#include <rendering/Texture.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/Compute.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>

namespace hyperion::v2 {

struct RenderCommand_UpdateEnvProbeDrawProxy;
struct RenderCommand_CreateCubemapBuffers;
struct RenderCommand_DestroyCubemapRenderPass;

class Framebuffer;

using renderer::Attachment;
using renderer::UniformBuffer;
using renderer::Image;

class EnvProbe
    : public EngineComponentBase<STUB_CLASS(EnvProbe)>,
      public HasDrawProxy<STUB_CLASS(EnvProbe)>,
      public RenderResource
{
    static const FixedArray<std::pair<Vector3, Vector3>, 6> cubemap_directions;

public:
    friend struct RenderCommand_UpdateEnvProbeDrawProxy;
    friend struct RenderCommand_CreateCubemapBuffers;
    friend struct RenderCommand_DestroyCubemapRenderPass;
    
    EnvProbe(
        const Handle<Scene> &parent_scene,
        const BoundingBox &aabb,
        const Extent2D &dimensions,
        bool is_ambient_probe
    );

    EnvProbe(const EnvProbe &other) = delete;
    EnvProbe &operator=(const EnvProbe &other) = delete;
    ~EnvProbe();

    bool IsAmbientProbe() const
        { return m_is_ambient_probe; }

    const Matrix4 &GetProjectionMatrix() const
        { return m_projection_matrix; }

    const FixedArray<Matrix4, 6> &GetViewMatrices() const
        { return m_view_matrices; }

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; SetNeedsUpdate(); }

    Handle<Texture> &GetTexture()
        { return m_texture; }

    const Handle<Texture> &GetTexture() const
        { return m_texture; }

    void Init();
    void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update();

    void Render(Frame *frame);

    void ComputeSH(Frame *frame, const Image *image, const ImageView *image_view);

    void UpdateRenderData(UInt probe_index);

private:
    void SetNeedsUpdate() { m_needs_update = true; }

    void CreateImagesAndBuffers();
    void CreateShader();
    void CreateFramebuffer();

    void CreateSHData();

    Handle<Scene> m_parent_scene;
    BoundingBox m_aabb;
    Extent2D m_dimensions;
    bool m_is_ambient_probe;

    Handle<Texture> m_texture;
    Handle<Framebuffer> m_framebuffer;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    Handle<Shader> m_shader;
    Handle<Scene> m_scene;

    CubemapUniforms m_cubemap_uniforms;
    FixedArray<GPUBufferRef, max_frames_in_flight> m_cubemap_render_uniform_buffers;
    
    ImageRef m_sh_image_final;
    ImageViewRef m_sh_image_view_final;

    GPUBufferRef m_sh_tiles_buffer;

    Handle<ComputePipeline> m_compute_sh;
    Handle<ComputePipeline> m_clear_sh;
    Handle<ComputePipeline> m_finalize_sh;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_compute_sh_descriptor_sets;

    Matrix4 m_projection_matrix;
    FixedArray<Matrix4, 6> m_view_matrices;

    bool m_needs_update;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
