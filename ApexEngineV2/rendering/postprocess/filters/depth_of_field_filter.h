#ifndef DEPTH_OF_FIELD_FILTER_H
#define DEPTH_OF_FIELD_FILTER_H

#include <array>
#include <memory>

#include "../post_filter.h"
#include "../../texture_2D.h"
#include "../../../math/vector3.h"
#include "../../../math/vector2.h"
#include "../../../math/matrix4.h"

namespace apex {

class DepthOfFieldFilter : public PostFilter {
public:
    DepthOfFieldFilter(float focus_range = 30.0f, float focus_scale = 1.0f);
    virtual ~DepthOfFieldFilter() = default;

    virtual void SetUniforms(Camera *cam) override;

private:
    float m_focus_range;
    float m_focus_scale;
};

} // namespace apex

#endif
