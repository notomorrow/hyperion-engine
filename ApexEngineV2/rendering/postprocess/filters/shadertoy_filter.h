#ifndef SHADERTOY_FILTER_H
#define SHADERTOY_FILTER_H

#include <array>
#include <memory>

#include "../post_filter.h"
#include "../../texture_2D.h"
#include "../../../math/vector3.h"
#include "../../../math/vector2.h"
#include "../../../math/matrix4.h"

namespace apex {

class ShadertoyFilter : public PostFilter {
public:
    ShadertoyFilter();
    virtual ~ShadertoyFilter() = default;

    virtual void SetUniforms(Camera *cam) override;
};

} // namespace apex

#endif
