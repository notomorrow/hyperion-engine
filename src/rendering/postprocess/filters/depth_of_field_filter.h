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
    DepthOfFieldFilter(float focus_range = 10.0f, float focus_scale = 1.0f);
    virtual ~DepthOfFieldFilter() = default;

    virtual void SetUniforms(Camera *cam) override;

private:
    float m_focus_range;
    float m_focus_scale;

    DeclaredUniform::Id_t m_uniform_camera_near_far,
        m_uniform_scale,
        m_uniform_focus_range,
        m_uniform_focus_scale,
        m_uniform_pixel_size;
};

} // namespace hyperion

#endif
