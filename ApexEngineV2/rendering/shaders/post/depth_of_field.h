#ifndef DEPTH_OF_FIELD_H
#define DEPTH_OF_FIELD_H

#include "../post_shader.h"

namespace apex {

class DepthOfFieldShader : public PostShader {
public:
    DepthOfFieldShader(const ShaderProperties &properties);
    virtual ~DepthOfFieldShader() = default;

    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};

} // namespace apex

#endif
