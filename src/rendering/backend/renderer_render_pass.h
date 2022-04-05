#ifndef HYPERION_RENDERER_RENDER_PASS_H
#define HYPERION_RENDERER_RENDER_PASS_H

#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "renderer_attachment.h"

#include <vulkan/vulkan.h>

#include <optional>

namespace hyperion {
namespace renderer {

class CommandBuffer;

class RenderPassAttachment;

enum class Stage {
    RENDER_PASS_STAGE_NONE    = 0,
    RENDER_PASS_STAGE_PRESENT = 1, /* for presentation on screen */
    RENDER_PASS_STAGE_SHADER  = 2, /* for use as a sampled texture in a shader */
    RENDER_PASS_STAGE_BLIT    = 3  /* for blitting into another framebuffer in a later stage */
};

enum class LoadOperation {
    UNDEFINED,
    NONE,
    CLEAR,
    LOAD
};

enum class StoreOperation {
    UNDEFINED,
    NONE,
    STORE
};

struct RefCount {
    size_t count = 0;
};

class RenderPassAttachmentRef {
    friend class RenderPassAttachment;
public:
    RenderPassAttachmentRef(
        RenderPassAttachment *attachment,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    RenderPassAttachmentRef(
        RenderPassAttachment *attachment,
        std::unique_ptr<ImageView> &&image_view,
        std::unique_ptr<Sampler> &&sampler,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    RenderPassAttachmentRef(const RenderPassAttachmentRef &other) = delete;
    RenderPassAttachmentRef &operator=(const RenderPassAttachmentRef &other) = delete;
    ~RenderPassAttachmentRef();

    inline RenderPassAttachment *GetAttachment() const { return m_attachment; }

    inline ImageView *GetImageView() const { return m_image_view.get(); }
    inline Sampler *GetSampler() const { return m_sampler.get(); }

    inline LoadOperation GetLoadOperation() const { return m_load_operation; }
    inline StoreOperation GetStoreOperation() const { return m_store_operation; }

    inline uint32_t GetBinding() const { return m_binding.value_or(UINT32_MAX); }
    inline void SetBinding(uint32_t binding) { m_binding = binding; }
    inline bool HasBinding() const { return m_binding.has_value(); }

    Image::InternalFormat GetFormat() const;
    bool IsDepthAttachment() const;

    VkAttachmentDescription GetAttachmentDescription() const;
    VkAttachmentReference GetAttachmentReference() const;

    void IncRef() const;
    void DecRef() const;


    Result Create(Device *device);
    Result Create(
        Device *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        size_t num_mipmaps,
        size_t num_faces);
    Result Destroy(Device *device);

    Result AddAttachmentRef(Device *device, StoreOperation store_operation, RenderPassAttachmentRef **out = nullptr);

private:
    static VkAttachmentLoadOp ToVkLoadOp(LoadOperation);
    static VkAttachmentStoreOp ToVkStoreOp(StoreOperation);
    
    VkImageLayout GetIntermediateLayout() const;

    RenderPassAttachment *m_attachment;
    std::unique_ptr<ImageView> m_image_view;
    std::unique_ptr<Sampler> m_sampler;
    
    LoadOperation m_load_operation;
    StoreOperation m_store_operation;
    std::optional<uint32_t> m_binding{};

    VkImageLayout m_initial_layout, m_final_layout;

    RefCount *m_ref_count = nullptr;
    bool m_is_created     = false;
};

class RenderPassAttachment {
    friend class RenderPassAttachmentRef;
public:
    RenderPassAttachment(std::unique_ptr<Image> &&image, Stage stage)
        : m_is_created(false),
          m_image(std::move(image)),
          m_stage(stage)
    {
    }

    RenderPassAttachment(const RenderPassAttachment &other) = delete;
    RenderPassAttachment &operator=(const RenderPassAttachment &other) = delete;
    ~RenderPassAttachment()
    {
        AssertThrowMsg(!m_is_created, "Attachment must not be in `created` state on destructor call");

        for (size_t i = 0; i < m_ref_counts.size(); i++) {
            --m_ref_counts[i]->count;

            AssertThrowMsg(
                m_ref_counts[i]->count == 0,
                "Expected ref count at %ull to be zero after decrement -- object still in use somewhere else.",
                i
            );
        }
    }

    inline Image *GetImage() const { return m_image.get(); }

    inline auto &GetAttachmentRefs() { return m_attachment_refs; }
    inline const auto &GetAttachmentRefs() const { return m_attachment_refs; }

    inline Image::InternalFormat GetFormat() const
        { return m_image ? m_image->GetTextureFormat() : Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE; }

    inline bool IsDepthAttachment() const
        { return m_image ? m_image->IsDepthStencilImage() : false; }

    Result AddAttachmentRef(
        Device *device,
        LoadOperation load_operation,
        StoreOperation store_operation,
        RenderPassAttachmentRef **out = nullptr
    )
    {
        auto attachment_ref = std::make_unique<RenderPassAttachmentRef>(this, load_operation, store_operation);

        if (out != nullptr) {
            *out = nullptr;
        }

        if (m_is_created) {
            HYPERION_BUBBLE_ERRORS(attachment_ref->Create(device));
        }
        
        m_ref_counts.push_back(new RefCount{1});
        attachment_ref->m_ref_count = m_ref_counts.back();
        m_attachment_refs.push_back(std::move(attachment_ref));

        if (out != nullptr) {
            *out = m_attachment_refs.back().get();
        }

        HYPERION_RETURN_OK;
    }

    Result AddAttachmentRef(
        Device *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        size_t num_mipmaps,
        size_t num_faces,
        LoadOperation load_operation,
        StoreOperation store_operation,
        RenderPassAttachmentRef **out = nullptr
    )
    {
        auto attachment_ref = std::make_unique<RenderPassAttachmentRef>(this, load_operation, store_operation);

        if (out != nullptr) {
            *out = nullptr;
        }

        if (m_is_created) {
            HYPERION_BUBBLE_ERRORS(attachment_ref->Create(
                device,
                image,
                format,
                aspect_flags,
                view_type,
                num_mipmaps,
                num_faces
            ));
        }
        
        m_ref_counts.push_back(new RefCount{1});
        attachment_ref->m_ref_count = m_ref_counts.back();
        m_attachment_refs.push_back(std::move(attachment_ref));

        if (out != nullptr) {
            *out = m_attachment_refs.back().get();
        }

        HYPERION_RETURN_OK;
    }

    Result RemoveAttachmentRef(Device *device, RenderPassAttachmentRef *attachment_ref)
    {
        AssertThrow(attachment_ref != nullptr, "Attachment reference cannot be null");

        const auto it = std::find_if(m_attachment_refs.begin(), m_attachment_refs.end(), [attachment_ref](const auto &item) {
            return item.get() == attachment_ref;
        });

        if (it == m_attachment_refs.end()) {
            return {Result::RENDERER_ERR, "Attachment ref not found"};
        }

        AssertThrowMsg(
            attachment_ref->m_ref_count->count == 1,
            "Expected ref count to be 1 before explicit removal -- still in use somewhere else."
        );

        const auto ref_count_index = it - m_attachment_refs.begin();

        AssertThrow(attachment_ref->m_ref_count == m_ref_counts[ref_count_index]);

        delete m_ref_counts[ref_count_index];
        m_ref_counts.erase(m_ref_counts.begin() + ref_count_index);

        HYPERION_BUBBLE_ERRORS((*it)->Destroy(device));
        m_attachment_refs.erase(it);

        HYPERION_RETURN_OK;
    }

    Result Create(Device *device)
    {
        AssertThrow(!m_is_created);
        AssertThrow(m_image != nullptr);

        auto result = Result::OK;

        HYPERION_BUBBLE_ERRORS(m_image->Create(device));

        m_is_created = true;

        for (auto &attachment_ref : m_attachment_refs) {
            HYPERION_PASS_ERRORS(attachment_ref->Create(device), result);
        }

        return result;
    }

    Result Destroy(Device *device)
    {
        AssertThrow(m_is_created);
        AssertThrow(m_image != nullptr);

        auto result = Result::OK;

        HYPERION_PASS_ERRORS(m_image->Destroy(device), result);

        m_is_created = false;

        for (auto &attachment_ref : m_attachment_refs) {
            HYPERION_PASS_ERRORS(attachment_ref->Destroy(device), result);
        }

        return result;
    }

private:

    VkImageLayout GetInitialLayout() const
        { return VK_IMAGE_LAYOUT_UNDEFINED; }

    VkImageLayout GetFinalLayout() const
    {
        switch (m_stage) {
        case Stage::RENDER_PASS_STAGE_NONE:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case Stage::RENDER_PASS_STAGE_PRESENT:
            return IsDepthAttachment()
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case Stage::RENDER_PASS_STAGE_SHADER:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        default:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    bool m_is_created;
    std::unique_ptr<Image> m_image;
    Stage m_stage;

    std::vector<std::unique_ptr<RenderPassAttachmentRef>> m_attachment_refs;
    std::vector<RefCount *> m_ref_counts;
};



class RenderPass {
    friend class FramebufferObject;
    friend class GraphicsPipeline;
public:

    enum class Mode {
        RENDER_PASS_INLINE = 0,
        RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
    };

    struct Attachment {
        Image::InternalFormat format;
    };

    RenderPass(Stage stage, Mode mode);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    inline Stage GetStage() const
        { return m_stage; }

    void AddRenderPassAttachmentRef(RenderPassAttachmentRef *attachment_ref)
    {
        attachment_ref->IncRef();

        m_render_pass_attachment_refs.push_back(attachment_ref);
    }

    inline auto &GetRenderPassAttachmentRefs() { return m_render_pass_attachment_refs; }
    inline const auto &GetRenderPassAttachmentRefs() const { return m_render_pass_attachment_refs; }

    inline VkRenderPass GetRenderPass() const { return m_render_pass; }

    Result Create(Device *device);
    Result Destroy(Device *device);

    void Begin(CommandBuffer *cmd,
        VkFramebuffer framebuffer,
        VkExtent2D extent);
    void End(CommandBuffer *cmd);

private:
    void CreateDependencies();

    inline void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.push_back(dependency); }

    Stage m_stage;
    Mode m_mode;
    std::vector<std::pair<uint32_t, Attachment>> m_attachments;

    std::vector<RenderPassAttachmentRef *> m_render_pass_attachment_refs;

    std::vector<VkSubpassDependency> m_dependencies;
    std::vector<VkClearValue> m_clear_values;

    VkRenderPass m_render_pass;
};

} // namespace renderer
} // namespace hyperion

#endif