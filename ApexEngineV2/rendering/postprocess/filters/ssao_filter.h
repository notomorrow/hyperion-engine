#ifndef SSAO_FILTER_H
#define SSAO_FILTER_H

#include <array>
#include <memory>

#include "../post_filter.h"
#include "../../texture_2D.h"
#include "../../../math/vector3.h"
#include "../../../math/vector2.h"
#include "../../../math/matrix4.h"

namespace hyperion {

class SSAOFilter : public PostFilter {
public:
    SSAOFilter();
    virtual ~SSAOFilter() = default;

    virtual void SetUniforms(Camera *cam) override;

private:
    std::array<Vector3, 32> m_kernel;
    Vector2 m_noise_scale;
    std::shared_ptr<Texture2D> m_noise_map;
};

} // namespace hyperion

#endif
