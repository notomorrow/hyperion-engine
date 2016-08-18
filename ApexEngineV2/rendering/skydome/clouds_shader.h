#ifndef CLOUDS_SHADER_H
#define CLOUDS_SHADER_H

#include "../camera/camera.h"
#include "../shader.h"
#include "../texture_2D.h"

namespace apex {
class CloudsShader : public Shader {
public:
    CloudsShader(const ShaderProperties &properties);

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Matrix4 &model, const Matrix4 &view, const Matrix4 &proj);

    void SetCloudColor(const Vector4 &cloud_color);
    void SetGlobalTime(float global_time);
    void SetCamera(Camera *camera);

private:
    std::shared_ptr<Texture2D> cloud_map;

    Vector4 _cloud_color;
    float _global_time;
    Camera *_camera;
};
}

#endif