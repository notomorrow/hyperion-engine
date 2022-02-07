#include "probe.h"

#include "../camera/perspective_camera.h"
#include "../../math/matrix_util.h"

namespace hyperion {
Probe::Probe(const Vector3 &origin, const BoundingBox &bounds, int width, int height, float near, float far)
    : m_origin(origin),
      m_bounds(bounds),
      m_width(width),
      m_height(height),
      m_near(near),
      m_far(far),
      m_directions({
        std::make_pair(Vector3(1, 0, 0), Vector3(0, -1, 0)),
        std::make_pair(Vector3(-1, 0, 0), Vector3(0, -1, 0)),
        std::make_pair(Vector3(0, 1, 0), Vector3(0, 0, 1)),
        std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, -1)),
        std::make_pair(Vector3(0, 0, 1), Vector3(0, -1, 0)),
        std::make_pair(Vector3(0, 0, -1), Vector3(0, -1, 0))
      })
{
    for (int i = 0; i < m_cameras.size(); i++) {
        ProbeRegion region;
        region.origin = m_origin;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;
        region.index = i;

        m_cameras[i] = new ProbeCamera(region, width, height, near, far);
    }
}

Probe::~Probe()
{
    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }
}

void Probe::SetBounds(const BoundingBox &bounds)
{
    m_bounds = bounds;

    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        cam->GetRegion().bounds = bounds;
    }
}

void Probe::SetOrigin(const Vector3 &origin)
{
    m_origin = origin;

    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        cam->GetRegion().origin = origin;
    }
}

void Probe::Begin()
{
    for (ProbeCamera *cam : m_cameras) {
        cam->Update(1); //todo
    }
}

void Probe::End()
{
}
} // namespace hyperion
