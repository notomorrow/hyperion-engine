#ifndef FXAA_FILTER_H
#define FXAA_FILTER_H

#include <array>
#include <memory>

#include "../post_filter.h"
#include "../../texture_2D.h"
#include "../../../math/vector3.h"
#include "../../../math/vector2.h"
#include "../../../math/matrix4.h"

namespace hyperion {

class FXAAFilter : public PostFilter {
public:
    FXAAFilter();
    virtual ~FXAAFilter() = default;

    virtual void SetUniforms(Camera *cam) override;
};

} // namespace hyperion

#endif
