#ifndef DEFERRED_RENDERING_FILTER_H
#define DEFERRED_RENDERING_FILTER_H

#include "../post_filter.h"

namespace hyperion {
class DeferredRenderingFilter : public PostFilter {
public:
    DeferredRenderingFilter();
    virtual ~DeferredRenderingFilter() = default;

    virtual void SetUniforms(Camera *cam) override;
};
} // namespace hyperion

#endif
