#ifndef HYPERION_V2_TEMPORAL_BLENDING_HPP
#define HYPERION_V2_TEMPORAL_BLENDING_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/Compute.hpp>

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

class Engine;

class TemporalBlending
{
public:
    struct ImageOutput
    {
        StorageImage image;
        ImageView image_view;

        void Create(Device *device)
        {
            HYPERION_ASSERT_RESULT(image.Create(device));
            HYPERION_ASSERT_RESULT(image_view.Create(device, &image));
        }

        void Destroy(Device *device)
        {
            HYPERION_ASSERT_RESULT(image.Destroy(device));
            HYPERION_ASSERT_RESULT(image_view.Destroy(device));
        }
    };

    TemporalBlending(
        const Extent2D &extent,
        const FixedArray<Image *, max_frames_in_flight> &input_images,
        const FixedArray<ImageView *, max_frames_in_flight> &input_image_views
    );
    TemporalBlending(const TemporalBlending &other) = delete;
    TemporalBlending &operator=(const TemporalBlending &other) = delete;
    ~TemporalBlending();

    ImageOutput &GetImageOutput(UInt frame_index)
        { return m_image_outputs[frame_index]; }

    const ImageOutput &GetImageOutput(UInt frame_index) const
        { return m_image_outputs[frame_index]; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void CreateImageOutputs(Engine *engine);
    void CreateDescriptorSets(Engine *engine);
    void CreateComputePipelines(Engine *engine);

    Extent2D m_extent;

    FixedArray<Image *, max_frames_in_flight> m_input_images;
    FixedArray<ImageView *, max_frames_in_flight> m_input_image_views;

    Handle<ComputePipeline> m_perform_blending;

    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    FixedArray<ImageOutput, max_frames_in_flight> m_image_outputs;
};

} // namespace hyperion::v2

#endif