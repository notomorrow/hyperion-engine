#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include "light_source.h"
#include "../../math/vector3.h"
#include "../../math/vector4.h"

namespace hyperion {
class DirectionalLight : public LightSource {
public:
    DirectionalLight();
    DirectionalLight(const Vector3 &direction, const Vector4 &color, float intensity = 1.0f);
    virtual ~DirectionalLight() = default;

    const Vector3 &GetDirection() const;
    void SetDirection(const Vector3 &dir);
    const Vector4 &GetColor() const;
    void SetColor(const Vector4 &col);
    inline float GetIntensity() const { return m_intensity; }
    inline void SetIntensity(float intensity) { m_intensity = intensity; }

    void Bind(int index, Shader *shader) const;

private:
    Vector3 direction;
    Vector4 color;
    float m_intensity;
};
}

#endif
