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
        Ref<RenderPass> render_pass;
        std::vector<Ref<Framebuffer>> framebuffers;
        std::vector<std::unique_ptr<renderer::Attachment>> m_attachments;
        std::vector<Ref<GraphicsPipeline>> m_graphics_pipelines;

        inline void AddGraphicsPipeline(Ref<GraphicsPipeline> &&graphics_pipeline)
        {
            graphics_pipeline.Init();

            m_graphics_pipelines.push_back(std::move(graphics_pipeline));
        }

        void AddFramebuffersToPipelines(Engine *engine);
        void CreateRenderPass(Engine *engine);
        void CreateFramebuffers(Engine *engine);
        void Destroy(Engine *engine);
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

    void AddFramebuffersToPipelines(Engine *engine);
    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::array<Bucket, GraphicsPipeline::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif