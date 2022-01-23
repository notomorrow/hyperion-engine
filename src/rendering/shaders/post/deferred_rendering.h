#ifndef DEFERRED_RENDERING_H
#define DEFERRED_RENDERING_H

#include "../post_shader.h"

namespace hyperion {
class DeferredRenderingShader : public PostShader {
public:
    DeferredRenderingShader(const ShaderProperties &properties);
    virtual ~DeferredRenderingShader() = default;

    virtual void ApplyMaterial(const Material &mat) override;
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace hyperion

#endif
