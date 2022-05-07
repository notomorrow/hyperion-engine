#ifndef HYPERION_V2_LIGHT_H
#define HYPERION_V2_LIGHT_H

#include <math/vector4.h>

namespace hyperion::v2 {

class Light {
public:
    Light();
    Light(const Light &other) = delete;
    Light &operator=(const Light &other) = delete;
    ~Light();

protected:
    Vector4 m_color;
    float   m_intensity;
};

class DirectionalLight : public Light {
public:
    DirectionalLight();
    DirectionalLight(const DirectionalLight &other) = delete;
    DirectionalLight &operator=(const DirectionalLight &other) = delete;
    ~DirectionalLight();
};

} // namespace hyperion::v2

#endif