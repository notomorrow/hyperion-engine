#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include "../lightsource.h"
#include "../../math/vector3.h"
#include "../../math/vector4.h"

namespace apex {
class DirectionalLight : public LightSource {
public:
    DirectionalLight();
    DirectionalLight(const Vector3 &direction, const Vector4 &color);

    const Vector3 &GetDirection() const;
    void SetDirection(const Vector3 &dir);
    const Vector4 &GetColor() const;
    void SetColor(const Vector4 &col);

    void BindLight(int index, Shader *shader);

private:
    Vector3 direction;
    Vector4 color;
};
}

#endif