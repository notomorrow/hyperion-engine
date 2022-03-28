#ifndef HYPERION_V2_RENDER_BUCKET_H
#define HYPERION_V2_RENDER_BUCKET_H

#include "graphics.h"
#include "util.h"

namespace hyperion::v2 {

class Engine;

class RenderBucketContainer {
public:
    using Bucket = ObjectHolder<GraphicsPipeline>;

    RenderBucketContainer();
    RenderBucketContainer(const RenderBucketContainer &other) = delete;
    RenderBucketContainer &operator=(const RenderBucketContainer &other) = delete;
    ~RenderBucketContainer() = default;

    inline auto &GetBuckets() { return m_buckets; }
    inline const auto &GetBuckets() const { return m_buckets; }

    inline Bucket &GetBucket(GraphicsPipeline::Bucket bucket) { return m_buckets[int(bucket)]; }
    inline const Bucket &GetBucket(GraphicsPipeline::Bucket bucket) const { return m_buckets[int(bucket)]; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::array<Bucket, 5> m_buckets;
};

} // namespace hyperion::v2

#endif