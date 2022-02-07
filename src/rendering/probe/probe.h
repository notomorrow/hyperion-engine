#ifndef PROBE_H
#define PROBE_H

#include "probe_camera.h"
#include "../../math/matrix4.h"
#include "../../math/vector3.h"

#include <memory>
#include <array>

namespace hyperion {
class Probe {
public:
    Probe(const Vector3 &origin, const BoundingBox &bounds, int width, int height, float near, float far);
    Probe(const Probe &other) = delete;
    Probe &operator=(const Probe &other) = delete;
    virtual ~Probe();

    inline const BoundingBox &GetBounds() const { return m_bounds; }
    void SetBounds(const BoundingBox &bounds);

    inline const Vector3 &GetOrigin() const { return m_origin; }
    void SetOrigin(const Vector3 &origin);

    inline const std::array<ProbeCamera *, 6> &GetCameras() const { return m_cameras; }

    inline int GetWidth() const { return m_width; }
    inline int GetHeight() const { return m_height; }
    inline float GetNear() const { return m_near; }
    inline float GetFar() const { return m_far; }

    void Begin();
    void End();

private:
    BoundingBox m_bounds;
    Vector3 m_origin;
    int m_width;
    int m_height;
    float m_near;
    float m_far;
    std::array<ProbeCamera*, 6> m_cameras;
    const std::array<std::pair<Vector3, Vector3>, 6> m_directions;
};
} // namespace hyperion

#endif
