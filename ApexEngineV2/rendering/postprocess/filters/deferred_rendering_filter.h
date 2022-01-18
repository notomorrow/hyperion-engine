#ifndef DEFERRED_RENDERING_FILTER_H
#define DEFERRED_RENDERING_FILTER_H

#include "../post_filter.h"

namespace apex {
class DeferredRenderingFilter : public PostFilter {
public:
    DeferredRenderingFilter();
    virtual ~DeferredRenderingFilter() = default;

    virtual void SetUniforms(Camera *cam) override;
};
} // namespace apex

#endif
