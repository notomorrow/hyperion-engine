#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "FullScreenPass.hpp"
#include "PostFX.hpp"
#include "Texture.hpp"
#include "Compute.hpp"
#include "IndirectDraw.hpp"

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentRef;

using DeferredFlagBits = UInt;

struct IndirectDrawState;

enum DeferredFlags : DeferredFlagBits {
    DEFERRED_FLAGS_NONE              = 0,
    DEFERRED_FLAGS_SSR_ENABLED       = 1 << 0,
    DEFERRED_FLAGS_VCT_ENABLED       = 1 << 1,
    DEFERRED_FLAGS_ENV_PROBE_ENABLED = 1 << 2
};

class ScreenspaceReflectionRenderer {
public:
    ScreenspaceReflectionRenderer(const Extent2D &extent);
    ~ScreenspaceReflectionRenderer();

    bool IsRendered() const { return m_is_rendered; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void CreateDescriptors(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    
    struct SSRImageOutput {
        std::unique_ptr<Image>     image;
        std::unique_ptr<ImageView> image_view;

        void Create(Device *device)
        {
            AssertThrow(image != nullptr);
            AssertThrow(image_view != nullptr);

            HYPERION_ASSERT_RESULT(image->Create(device));
            HYPERION_ASSERT_RESULT(image_view->Create(device, image.get()));
        }

        void Destroy(Device *device)
        {
            AssertThrow(image != nullptr);
            AssertThrow(image_view != nullptr);

            HYPERION_ASSERT_RESULT(image->Destroy(device));
            HYPERION_ASSERT_RESULT(image_view->Destroy(device));
        }
    };

    Extent2D                                                        m_extent;

    std::array<std::array<SSRImageOutput, 4>, max_frames_in_flight> m_ssr_image_outputs;
    std::array<SSRImageOutput, max_frames_in_flight>                m_ssr_radius_output;

    Ref<ComputePipeline>                                            m_ssr_write_uvs;
    Ref<ComputePipeline>                                            m_ssr_sample;
    Ref<ComputePipeline>                                            m_ssr_blur_hor;
    Ref<ComputePipeline>                                            m_ssr_blur_vert;

    bool                                                            m_is_rendered;
};

class DepthPyramidRenderer {
public:
    DepthPyramidRenderer();
    ~DepthPyramidRenderer();

    auto &GetResults()             { return m_depth_pyramid_results; }
    const auto &GetResults() const { return m_depth_pyramid; }

    auto &GetMips()                { return m_depth_pyramid_mips; }
    const auto &GetMips() const    { return m_depth_pyramid_mips; }

    bool IsRendered() const        { return m_is_rendered; }

    void Create(Engine *engine, const AttachmentRef *depth_attachment_ref);
    void Destroy(Engine *engine);

    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    const AttachmentRef                                                       *m_depth_attachment_ref;

    FixedArray<std::unique_ptr<Image>, max_frames_in_flight>                   m_depth_pyramid;
    FixedArray<std::unique_ptr<ImageView>, max_frames_in_flight>               m_depth_pyramid_results;
    FixedArray<DynArray<std::unique_ptr<ImageView>>, max_frames_in_flight>     m_depth_pyramid_mips;
    FixedArray<DynArray<std::unique_ptr<DescriptorSet>>, max_frames_in_flight> m_depth_pyramid_descriptor_sets;
    std::unique_ptr<Sampler>                                                   m_depth_pyramid_sampler;

    Ref<ComputePipeline>                                                       m_generate_depth_pyramid;

    bool                                                                       m_is_rendered;
};

class DeferredPass : public FullScreenPass {
    friend class DeferredRenderer;
public:
    DeferredPass(bool is_indirect_pass);
    DeferredPass(const DeferredPass &other) = delete;
    DeferredPass &operator=(const DeferredPass &other) = delete;
    ~DeferredPass();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Record(Engine *engine, UInt frame_index);
    void Render(Engine *engine, Frame *frame);

private:
    const bool m_is_indirect_pass;
};

class DeferredRenderer {
    static constexpr bool ssr_enabled = true;

public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    PostProcessing &GetPostProcessing()              { return m_post_processing; }
    const PostProcessing &GetPostProcessing() const  { return m_post_processing; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:

    void RenderOpaqueObjects(Engine *engine, Frame *frame);
    void RenderTranslucentObjects(Engine *engine, Frame *frame);

    DeferredPass                                                               m_indirect_pass;
    DeferredPass                                                               m_direct_pass;
    PostProcessing                                                             m_post_processing;

    ScreenspaceReflectionRenderer                                              m_ssr;
    DepthPyramidRenderer                                                       m_dpr;

    FixedArray<Ref<Texture>, max_frames_in_flight>                             m_mipmapped_results;
    std::unique_ptr<Sampler>                                                   m_sampler;
    std::unique_ptr<Sampler>                                                   m_depth_sampler;

    IndirectDrawState                                                          m_indirect_draw_state;
};

} // namespace hyperion::v2

#endif