#ifndef SKYDOME_SHADER_H
#define SKYDOME_SHADER_H

#include "../camera/camera.h"
#include "../shader.h"
#include "../texture_2D.h"
#include "../../math/transform.h"
#include "../../math/vector3.h"
#include "../../math/vector4.h"

namespace hyperion {
class SkydomeShader : public Shader {
public:
    SkydomeShader(const ShaderProperties &properties);
    virtual ~SkydomeShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

    void SetGlobalTime(float global_time);

private:
    std::shared_ptr<Texture2D> noise_map;

    float m_global_time;

    Vector4 sun_color;
    Vector3 wavelength;
    Vector3 inv_wavelength4;

    bool has_clouds;
    int num_samples;
    float Kr;
    float KrESun;
    float Kr4PI;
    float Km;
    float KmESun;
    float Km4PI;
    float ESun;
    float G;
    float inner_radius;
    float scale;
    float scale_depth;
    float scale_over_scale_depth;
    float exposure;
};
} // namespace hyperion

#endif
