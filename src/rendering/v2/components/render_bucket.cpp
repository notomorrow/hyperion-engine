#include "render_bucket.h"

namespace hyperion::v2 {

RenderBucketContainer::RenderBucketContainer()
{
    for (auto &bucket : m_buckets) {
        bucket = Bucket{.defer_create = true};
    }
}

void RenderBucketContainer::Create(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.CreateAll(engine);
    }
}

void RenderBucketContainer::Destroy(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.RemoveAll(engine);
    }
}

} // namespace hyperion::v2