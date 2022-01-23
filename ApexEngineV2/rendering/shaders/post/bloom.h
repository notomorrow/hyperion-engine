#ifndef BLOOM_H
#define BLOOM_H

#include "../post_shader.h"

namespace hyperion {

class BloomShader : public PostShader {
public:
    BloomShader(const ShaderProperties &properties);
    virtual ~BloomShader() = default;

    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};

} // namespace hyperion

#endif
