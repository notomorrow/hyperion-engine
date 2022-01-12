#ifndef CLOUDS_SHADER_H
#define CLOUDS_SHADER_H

#include "../camera/camera.h"
#include "../shader.h"
#include "../texture_2D.h"

namespace apex {
class CloudsShader : public Shader {
public:
    CloudsShader(const ShaderProperties &properties);
    virtual ~CloudsShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

    void SetCloudColor(const Vector4 &cloud_color);
    void SetGlobalTime(float global_time);

private:
    std::shared_ptr<Texture2D> cloud_map;

    Vector4 m_cloud_color;
    float m_global_time;
};
} // namespace apex

#endif
