#ifndef GAMMA_CORRECTION_FILTER_H
#define GAMMA_CORRECTION_FILTER_H

#include "../post_filter.h"

namespace hyperion {
class GammaCorrectionFilter : public PostFilter {
public:
    GammaCorrectionFilter();
    virtual ~GammaCorrectionFilter() = default;

    virtual void SetUniforms(Camera *cam) override;
};
} // namespace hyperion

#endif
