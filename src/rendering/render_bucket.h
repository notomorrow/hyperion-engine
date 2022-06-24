#ifndef HYPERION_V2_RENDER_BUCKET_H
#define HYPERION_V2_RENDER_BUCKET_H

#include <util/defines.h>

namespace hyperion::v2 {

enum Bucket {
    BUCKET_SWAPCHAIN = 0, /* Main swapchain */
    BUCKET_PREPASS,       /* Pre-pass / buffer items */
    BUCKET_VOXELIZER,
    /* === Scene objects === */
    BUCKET_OPAQUE,        /* Opaque items */
    BUCKET_TRANSLUCENT,   /* Transparent - rendering on top of opaque objects */
    BUCKET_PARTICLE,      /* Specialized particle bucket */
    BUCKET_SKYBOX,        /* Rendered without depth testing/writing, and rendered first */
    BUCKET_MAX
};

HYP_FORCE_INLINE
static inline bool BucketRayTestsEnabled(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE
static inline bool BucketFrustumCullingEnabled(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT
        || bucket == BUCKET_PARTICLE;
}

} // namespace hyperion::v2

#endif