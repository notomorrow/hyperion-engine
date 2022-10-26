#ifndef HYPERION_V2_MOTION_VECTORS_HPP
#define HYPERION_V2_MOTION_VECTORS_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>
#include <rendering/Compute.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <math/Matrix4.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::UniformBuffer;
using renderer::Frame;
using renderer::Extent2D;
using renderer::ShaderMat4;
using renderer::ShaderVec2;
using renderer::Result;

class Engine;

class MotionVectors
{
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

public:
    MotionVectors(const Extent2D &extent);
    ~MotionVectors();

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
    void CreateImages(Engine *engine);
    void CreateUniformBuffers(Engine *engine);
    void CreateDescriptorSets(Engine *engine);
    void CreateComputePipelines(Engine *engine);

    FixedArray<ImageOutput, max_frames_in_flight> m_image_outputs;
    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;

    Image *m_depth_image;
    ImageView *m_depth_image_view;
    
    UniquePtr<Image> m_previous_depth_image;
    UniquePtr<ImageView> m_previous_depth_image_view;

    FixedArray<UniquePtr<UniformBuffer>, max_frames_in_flight> m_uniform_buffers;

    struct CameraMatrixCache
    {
        Matrix4 view;
        Matrix4 projection;
        Matrix4 inverse_view;
        Matrix4 inverse_projection;
    };

    CameraMatrixCache m_cached_matrices;

    Handle<ComputePipeline> m_calculate_motion_vectors;
};

} // namespace hyperion::v2

#endif