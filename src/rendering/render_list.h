#ifndef HYPERION_V2_RENDER_LIST_H
#define HYPERION_V2_RENDER_LIST_H

#include <core/containers.h>
#include <rendering/graphics.h>
#include <rendering/default_formats.h>
#include <scene/scene.h>
#include <types.h>

#include <mutex>
#include <array>

namespace hyperion::v2 {

class Engine;

class RenderListContainer {
public:
    static const std::array<TextureFormatDefault, 5> gbuffer_textures;
    
    class RenderListBucket {
        Bucket                                   bucket{BUCKET_OPAQUE};
        Ref<RenderPass>                          render_pass;
        std::vector<Ref<Framebuffer>>            framebuffers;
        std::vector<std::unique_ptr<Attachment>> attachments;
        std::vector<Ref<GraphicsPipeline>>       graphics_pipelines;
        std::vector<Ref<GraphicsPipeline>>       graphics_pipelines_pending_addition;
        std::atomic_bool                         graphics_pipelines_changed{false};
        std::mutex                               graphics_pipelines_mutex;
        ObserverNotifier<Ref<GraphicsPipeline>>  m_graphics_pipeline_notifier;

    public:
        RenderListBucket();
        ~RenderListBucket();

        Bucket GetBucket() const                                                           { return bucket; }
        void SetBucket(Bucket bucket)                                                      { this->bucket = bucket; }
                                                                                           
        Ref<RenderPass> &GetRenderPass()                                                   { return render_pass; }
        const Ref<RenderPass> &GetRenderPass() const                                       { return render_pass; }
                                                                                           
        std::vector<Ref<Framebuffer>> &GetFramebuffers()                                   { return framebuffers; }
        const std::vector<Ref<Framebuffer>> &GetFramebuffers() const                       { return framebuffers; }
                                                                                           
        std::vector<Ref<GraphicsPipeline>> &GetGraphicsPipelines()                         { return graphics_pipelines; }
        const std::vector<Ref<GraphicsPipeline>> &GetGraphicsPipelines() const             { return graphics_pipelines; }

        ObserverNotifier<Ref<GraphicsPipeline>> &GetGraphicsPipelineNotifier()             { return m_graphics_pipeline_notifier; }
        const ObserverNotifier<Ref<GraphicsPipeline>> &GetGraphicsPipelineNotifier() const { return m_graphics_pipeline_notifier; }

        bool IsRenderableBucket() const
        {
            return bucket == Bucket::BUCKET_OPAQUE
                || bucket == Bucket::BUCKET_TRANSLUCENT
                || bucket == Bucket::BUCKET_SKYBOX
                || bucket == Bucket::BUCKET_PARTICLE;
        }

        void AddGraphicsPipeline(Ref<GraphicsPipeline> &&graphics_pipeline);
        void AddPendingGraphicsPipelines(Engine *engine);
        void AddFramebuffersToPipelines();
        void AddFramebuffersToPipeline(Ref<GraphicsPipeline> &pipeline);
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

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void AddPendingGraphicsPipelines(Engine *engine);
    void AddFramebuffersToPipelines(Engine *engine);

private:
    std::array<RenderListBucket, Bucket::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif