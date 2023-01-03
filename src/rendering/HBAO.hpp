#ifndef HYPERION_V2_HBAO_HPP
#define HYPERION_V2_HBAO_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Compute.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::StorageImage;
using renderer::ImageView;
using renderer::Frame;
using renderer::Extent2D;
using renderer::Result;

class Engine;

struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

class HBAO
{
    static constexpr bool blur_result = false;

public:
    friend struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

    HBAO(const Extent2D &extent);
    HBAO(const HBAO &other) = delete;
    HBAO &operator=(const HBAO &other) = delete;
    ~HBAO();

    void Create();
    void Destroy();
    
    void Render(Frame *frame);

private:
    void CreateImages();
    void CreatePass();
    void CreateTemporalBlending();
    void CreateDescriptorSets();
    void CreateBlurComputeShaders();
    
    struct ImageOutput
    {
        StorageImage image;
        ImageView image_view;

        ImageOutput(StorageImage &&image)
            : image(std::move(image))
        {
        }

        ImageOutput(const ImageOutput &other) = delete;
        ImageOutput(ImageOutput &&other) noexcept
            : image(std::move(other.image)),
              image_view(std::move(other.image_view))
        {
        }

        ~ImageOutput() = default;

        Result Create(Device *device);
        Result Destroy(Device *device);
    };

    FixedArray<ImageOutput, max_frames_in_flight> m_image_outputs;
    FixedArray<FixedArray<ImageOutput, 2>, max_frames_in_flight> m_blur_image_outputs;

    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    FixedArray<FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight>, 2> m_blur_descriptor_sets;

    UniquePtr<FullScreenPass> m_hbao_pass;
    Handle<ComputePipeline> m_blur_hor;
    Handle<ComputePipeline> m_blur_vert;
    UniquePtr<TemporalBlending> m_temporal_blending;
};

} // namespace hyperion::v2

#endif