#ifndef HYPERION_V2_RENDER_LIST_H
#define HYPERION_V2_RENDER_LIST_H

#include "containers.h"
#include "graphics.h"
#include "../scene/scene.h"

namespace hyperion::v2 {

class Engine;


class RenderListContainer {
public:
    struct RenderListBucket {
        Bucket bucket;
        Ref<RenderPass> render_pass;
        std::vector<Ref<Framebuffer>> framebuffers;
        std::vector<std::unique_ptr<renderer::Attachment>> attachments;
        std::vector<Ref<GraphicsPipeline>> graphics_pipelines;

        inline void AddGraphicsPipeline(Ref<GraphicsPipeline> &&graphics_pipeline)
        {
            graphics_pipeline.Init();

            graphics_pipelines.push_back(std::move(graphics_pipeline));
        }

        void AddFramebuffersToPipelines(Engine *engine);
        void CreateRenderPass(Engine *engine);
        void CreateFramebuffers(Engine *engine);
        void Destroy(Engine *engine);
    };

    RenderListContainer();
    RenderListContainer(const RenderListContainer &other) = delete;
    RenderListContainer &operator=(const RenderListContainer &other) = delete;
    ~RenderListContainer() = default;

    inline auto &GetBuckets()
        { return m_buckets; }
    inline const auto &GetBuckets() const
        { return m_buckets; }

    inline RenderListBucket &Get(Bucket bucket)
        { return m_buckets[int(bucket)]; }
    inline const RenderListBucket &Get(Bucket bucket) const
        { return m_buckets[int(bucket)]; }

    inline RenderListBucket &operator[](Bucket bucket)
        { return Get(bucket); }

    inline const RenderListBucket &operator[](Bucket bucket) const
        { return Get(bucket); }

    void AddFramebuffersToPipelines(Engine *engine);
    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::array<RenderListBucket, Bucket::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif