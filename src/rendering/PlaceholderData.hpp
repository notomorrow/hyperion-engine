#ifndef HYPERION_V2_DUMMY_DATA_H
#define HYPERION_V2_DUMMY_DATA_H

#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <math/MathUtil.hpp>

#include <Threads.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::TextureImage2D;
using renderer::TextureImage3D;
using renderer::TextureImageCube;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::GPUBufferType;

class Engine;
class PlaceholderData
{
public:
    PlaceholderData();
    PlaceholderData(const PlaceholderData &other)               = delete;
    PlaceholderData(PlaceholderData &&other)                    = delete;
    PlaceholderData &operator=(const PlaceholderData &other)    = delete;
    PlaceholderData &operator=(PlaceholderData &&other)         = delete;
    ~PlaceholderData();

#define HYP_DEF_DUMMY_DATA(type, getter, member) \
    public: \
        type##Ref &Get##getter() { return member; } \
        const type##Ref &Get##getter() const { return member; } \
    private: \
        type##Ref member

    HYP_DEF_DUMMY_DATA(Image, Image2D1x1R8, m_image_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView2D1x1R8, m_image_view_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView3D1x1x1R8, m_image_view_3d_1x1x1_r8);
    HYP_DEF_DUMMY_DATA(Image, Image3D1x1x1R8, m_image_3d_1x1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView3D1x1x1R8Storage, m_image_view_3d_1x1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(Image, Image3D1x1x1R8Storage, m_image_3d_1x1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(Image, ImageCube1x1R8, m_image_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageViewCube1x1R8, m_image_view_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinear, m_sampler_linear);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinearMipmap, m_sampler_linear_mipmap);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerNearest, m_sampler_nearest);

#undef HYP_DEF_DUMMY_DATA

public:
    void Create();
    void Destroy();

    /*! \brief Get or create a buffer of at least the given size */
    const GPUBufferRef &GetOrCreateBuffer(Device *device, GPUBufferType buffer_type, SizeType required_size)
    {
        Threads::AssertOnThread(THREAD_RENDER);

        if (!m_buffers.Contains(buffer_type)) {
            m_buffers.Set(buffer_type, { });
        }

        auto &buffer_container = m_buffers.At(buffer_type);

        auto it = buffer_container.LowerBound(required_size);

        if (it != buffer_container.End()) {
            return it->second;
        }

        const auto required_size_pow2 = MathUtil::NextPowerOf2(required_size);

        auto buffer = MakeRenderObject<renderer::GPUBuffer>(buffer_type);
        HYPERION_ASSERT_RESULT(buffer->Create(device, required_size_pow2));

        if (buffer->IsCPUAccessible()) {
            buffer->Memset(device, required_size_pow2, 0x00); // fill with zeros
        }

        const auto insert_result = buffer_container.Insert(required_size_pow2, std::move(buffer));
        AssertThrow(insert_result.second); // was inserted

        // TODO: GC of these buffers

        return insert_result.first->second;
    }

private:
    FlatMap<GPUBufferType, FlatMap<SizeType, GPUBufferRef>> m_buffers;
};

} // namespace hyperion::v2

#endif
