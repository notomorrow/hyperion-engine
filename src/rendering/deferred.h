#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "full_screen_pass.h"
#include "post_fx.h"
#include "renderer.h"
#include "texture.h"
#include "compute.h"

#include <rendering/backend/renderer_frame.h>
#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_image_view.h>
#include <rendering/backend/renderer_sampler.h>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;

class DeferredPass : public FullScreenPass {
    friend class DeferredRenderer;
public:
    DeferredPass();
    DeferredPass(const DeferredPass &other) = delete;
    DeferredPass &operator=(const DeferredPass &other) = delete;
    ~DeferredPass();

    auto &GetMipmappedResults()            { return m_mipmapped_results; }
    auto &GetSampler()                     { return m_sampler; }

    auto &GetSSRImageOutputs()             { return m_ssr_image_outputs; }
    const auto &GetSSRImageOutputs() const { return m_ssr_image_outputs; }

    void CreateShader(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
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

    std::array<Ref<Texture>, max_frames_in_flight>                  m_mipmapped_results;
    std::unique_ptr<Sampler>                                        m_sampler;
    std::array<std::array<SSRImageOutput, 4>, max_frames_in_flight> m_ssr_image_outputs;
    std::array<SSRImageOutput, max_frames_in_flight>                m_ssr_radius_output;

    Ref<ComputePipeline>                                            m_ssr_write_uvs;
    Ref<ComputePipeline>                                            m_ssr_sample;
    Ref<ComputePipeline>                                            m_ssr_blur_hor;
    Ref<ComputePipeline>                                            m_ssr_blur_vert;
};

class DeferredRenderer : public Renderer {
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    DeferredPass &GetPass()                          { return m_pass; }
    const DeferredPass &GetPass() const              { return m_pass; }

    PostProcessing &GetPostProcessing()              { return m_post_processing; }
    const PostProcessing &GetPostProcessing() const  { return m_post_processing; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    void RenderOpaqueObjects(Engine *engine, Frame *frame);
    void RenderTranslucentObjects(Engine *engine, Frame *frame);

    DeferredPass   m_pass;
    PostProcessing m_post_processing;
};

} // namespace hyperion::v2

#endif