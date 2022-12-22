#ifndef HYPERION_V2_TEMPORAL_BLENDING_HPP
#define HYPERION_V2_TEMPORAL_BLENDING_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/Compute.hpp>
#include <rendering/Framebuffer.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentRef;
using renderer::Extent2D;
using renderer::Result;

class Engine;

struct RenderCommand_CreateTemporalImageOutputs;

enum class TemporalBlendTechnique
{
    TECHNIQUE_0,
    TECHNIQUE_1,
    TECHNIQUE_2,
    TECHNIQUE_3
};

class TemporalBlending
{
public:
    friend struct RenderCommand_CreateTemporalImageOutputs;

    struct ImageOutput
    {
        ImageRef image;
        ImageViewRef image_view;

        Result Create(Device *device)
        {
            AssertThrow(image.IsValid());
            AssertThrow(image_view.IsValid());

            HYPERION_BUBBLE_ERRORS(image->Create(device));
            HYPERION_BUBBLE_ERRORS(image_view->Create(device, image.Get()));

            HYPERION_RETURN_OK;
        }
    };

    TemporalBlending(
        const Extent2D &extent,
        TemporalBlendTechnique technique,
        const FixedArray<ImageView *, max_frames_in_flight> &input_image_views
    );

    TemporalBlending(
        const Extent2D &extent,
        InternalFormat image_format,
        TemporalBlendTechnique technique,
        const Handle<Framebuffer> &input_framebuffer
    );

    TemporalBlending(
        const Extent2D &extent,
        InternalFormat image_format,
        TemporalBlendTechnique technique,
        const FixedArray<ImageView *, max_frames_in_flight> &input_image_views
    );

    TemporalBlending(const TemporalBlending &other) = delete;
    TemporalBlending &operator=(const TemporalBlending &other) = delete;
    ~TemporalBlending();

    ImageOutput &GetImageOutput(UInt frame_index)
        { return m_image_outputs[frame_index]; }

    const ImageOutput &GetImageOutput(UInt frame_index) const
        { return m_image_outputs[frame_index]; }

    void Create();
    void Destroy();

    void Render(Frame *frame);

private:
    void CreateImageOutputs();
    void CreateDescriptorSets();
    void CreateComputePipelines();

    Extent2D m_extent;
    InternalFormat m_image_format;
    TemporalBlendTechnique m_technique;

    FixedArray<ImageView *, max_frames_in_flight> m_input_image_views;

    Handle<ComputePipeline> m_perform_blending;

    FixedArray<DescriptorSetRef, max_frames_in_flight> m_descriptor_sets;
    FixedArray<ImageOutput, max_frames_in_flight> m_image_outputs;

    Handle<Framebuffer> m_input_framebuffer;
};

} // namespace hyperion::v2

#endif