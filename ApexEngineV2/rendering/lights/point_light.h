#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include "../lightsource.h"
#include "../../math/vector3.h"
#include "../../math/vector4.h"

namespace hyperion {
class PointLight : public LightSource {
public:
    PointLight();
    PointLight(const PointLight &other);
    PointLight(const Vector3 &position, const Vector4 &color, float radius);

    inline const Vector3 &GetPosition() const { return m_position; }
    inline void SetPosition(const Vector3 &position) { m_position = position; }
    inline const Vector4 &GetColor() const { return m_color; }
    inline void SetColor(const Vector4 &color) { m_color = color; }
    inline float GetRadius() const { return m_radius; }
    inline void SetRadius(float radius) { m_radius = radius; }

    void Bind(int index, Shader *shader);

private:
    Vector3 m_position;
    Vector4 m_color;
    float m_radius;
};
}

#endif
