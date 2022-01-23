#ifndef DEPTH_OF_FIELD_FILTER_H
#define DEPTH_OF_FIELD_FILTER_H

#include <array>
#include <memory>

#include "../post_filter.h"
#include "../../texture_2D.h"
#include "../../../math/vector3.h"
#include "../../../math/vector2.h"
#include "../../../math/matrix4.h"

namespace hyperion {

class DepthOfFieldFilter : public PostFilter {
public:
    DepthOfFieldFilter(float focus_range = 40.0f, float focus_scale = 8.0f);
    virtual ~DepthOfFieldFilter() = default;

    virtual void SetUniforms(Camera *cam) override;

private:
    float m_focus_range;
    float m_focus_scale;
};

} // namespace hyperion

#endif
