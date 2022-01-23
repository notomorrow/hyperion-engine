#ifndef PROBE_H
#define PROBE_H

#include "../../math/matrix4.h"
#include "../../math/vector3.h"

#include <memory>
#include <array>

namespace hyperion {
class Probe {
public:
    Probe(const Vector3 &origin, int width, int height, float near, float far);
    virtual ~Probe() = default;

    inline const Vector3 &GetOrigin() const { return m_origin; }
    inline void SetOrigin(const Vector3 &origin) { m_origin = origin; }

    inline int GetWidth() const { return m_width; }
    inline int GetHeight() const { return m_height; }
    inline float GetNear() const { return m_near; }
    inline float GetFar() const { return m_far; }

    inline const std::array<Matrix4, 6> &GetMatrices() const { return m_matrices; }
    inline std::array<Matrix4, 6> &GetMatrices() { return m_matrices; }

    void Begin();
    void End();

private:
    Vector3 m_origin;
    int m_width;
    int m_height;
    float m_near;
    float m_far;
    Matrix4 m_proj_matrix;
    std::array<Matrix4, 6> m_matrices;
    std::array<std::pair<Vector3, Vector3>, 6> m_directions;

    void UpdateMatrices();
};
} // namespace hyperion

#endif
