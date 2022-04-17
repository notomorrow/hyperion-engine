#ifndef HYPERION_V2_RENDER_LIST_H
#define HYPERION_V2_RENDER_LIST_H

#include "graphics.h"
#include "containers.h"

namespace hyperion::v2 {

class Engine;

class RenderList {
public:
    struct Bucket {
        GraphicsPipeline::Bucket bucket;
        ObjectHolder<GraphicsPipeline> pipelines;
        Ref<RenderPass> render_pass;
        std::vector<Ref<Framebuffer>> framebuffers;
        std::vector<std::unique_ptr<renderer::Attachment>> m_attachments;

        void CreatePipelines(Engine *engine);
        void CreateRenderPass(Engine *engine);
        void CreateFramebuffers(Engine *engine);
        void Destroy(Engine *engine);

        void Begin(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index);
        void End(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index);
    };

    RenderList();
    RenderList(const RenderList &other) = delete;
    RenderList &operator=(const RenderList &other) = delete;
    ~RenderList() = default;

    inline auto &GetBuckets()
        { return m_buckets; }
    inline const auto &GetBuckets() const
        { return m_buckets; }

    inline Bucket &Get(GraphicsPipeline::Bucket bucket)
        { return m_buckets[int(bucket)]; }
    inline const Bucket &Get(GraphicsPipeline::Bucket bucket) const
        { return m_buckets[int(bucket)]; }

    inline Bucket &operator[](GraphicsPipeline::Bucket bucket)
        { return Get(bucket); }

    inline const Bucket &operator[](GraphicsPipeline::Bucket bucket) const
        { return Get(bucket); }

    void CreatePipelines(Engine *engine);
    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::array<Bucket, GraphicsPipeline::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif