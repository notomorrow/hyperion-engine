#ifndef HYPERION_RENDERER_BACKEND_VULKAN_ATTACHMENT_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_ATTACHMENT_HPP

#include <core/Containers.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/Platform.hpp>

#include <math/MathUtil.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
class AttachmentUsage<Platform::VULKAN>
{
public:
    AttachmentUsage(
        AttachmentRef<Platform::VULKAN> attachment,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    AttachmentUsage(
        AttachmentRef<Platform::VULKAN> attachment,
        ImageViewRef<Platform::VULKAN> image_view,
        SamplerRef<Platform::VULKAN> sampler,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    AttachmentUsage(const AttachmentUsage &other)               = delete;
    AttachmentUsage &operator=(const AttachmentUsage &other)    = delete;
    ~AttachmentUsage();

    const AttachmentRef<Platform::VULKAN> &GetAttachment() const
        { return m_attachment; }

    const ImageViewRef<Platform::VULKAN> &GetImageView() const
        { return m_image_view; }

    const SamplerRef<Platform::VULKAN> &GetSampler() const
        { return m_sampler; }

    LoadOperation GetLoadOperation() const { return m_load_operation; }
    StoreOperation GetStoreOperation() const { return m_store_operation; }

    uint GetBinding() const { return m_binding; }
    void SetBinding(uint binding) { m_binding = binding; }
    bool HasBinding() const { return m_binding != UINT_MAX; }

    InternalFormat GetFormat() const;
    bool IsDepthAttachment() const;

    bool AllowBlending() const
        { return m_allow_blending; }

    void SetAllowBlending(bool allow_blending)
        { m_allow_blending = allow_blending; }

    VkAttachmentDescription GetAttachmentDescription() const;
    VkAttachmentReference GetHandle() const;
    
    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);

private:
    AttachmentRef<Platform::VULKAN> m_attachment;
    ImageViewRef<Platform::VULKAN>  m_image_view;
    SamplerRef<Platform::VULKAN>    m_sampler;
    
    LoadOperation                   m_load_operation;
    StoreOperation                  m_store_operation;
    uint                            m_binding = MathUtil::MaxSafeValue<uint>();

    bool                            m_image_view_owned = false;
    bool                            m_sampler_owned = false;

    bool                            m_allow_blending = true;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
