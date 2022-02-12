#ifndef LIGHT_VOLUME_RENDERER_SHADER_H
#define LIGHT_VOLUME_RENDERER_SHADER_H

#include "../cubemap_renderer_shader.h"

namespace hyperion {
class LightVolumeRendererShader : public CubemapRendererShader {
public:
    LightVolumeRendererShader(const ShaderProperties &properties);
    virtual ~LightVolumeRendererShader() = default;
};
} // namespace hyperion

#endif
