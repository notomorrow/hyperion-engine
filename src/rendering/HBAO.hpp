#ifndef HYPERION_V2_HBAO_HPP
#define HYPERION_V2_HBAO_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>

#include <rendering/Compute.hpp>

namespace hyperion::v2 {

using renderer::StorageImage;
using renderer::ImageView;
using renderer::Frame;
using renderer::Extent2D;
using renderer::Result;

class Engine;

class HBAO
{
public:
    HBAO(const Extent2D &extent);
    ~HBAO();

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    
    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void CreateImages(Engine *engine);
    void CreateDescriptorSets(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    
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

    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;

    Handle<ComputePipeline> m_compute_hbao;
    Handle<ComputePipeline> m_denoiser;
};

} // namespace hyperion::v2

#endif