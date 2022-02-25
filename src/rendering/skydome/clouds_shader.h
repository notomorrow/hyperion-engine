#ifndef CLOUDS_SHADER_H
#define CLOUDS_SHADER_H

#include "../camera/camera.h"
#include "../shader.h"
#include "../texture_2D.h"

namespace hyperion {
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

    DeclaredUniform::Id_t m_uniform_cloud_map,
        m_uniform_cloud_color,
        m_uniform_global_time;

    Vector4 m_cloud_color;
    float m_global_time;
};
} // namespace hyperion

#endif
