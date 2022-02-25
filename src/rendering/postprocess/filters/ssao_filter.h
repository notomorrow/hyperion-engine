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
    SSAOFilter(float strength = 2.0);
    virtual ~SSAOFilter() = default;

    virtual void SetUniforms(Camera *cam) override;

private:
    Shader::DeclaredUniform::Id_t m_uniform_strength;

    float m_strength;
};

} // namespace hyperion

#endif
