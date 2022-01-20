#include "probe.h"

#include "../camera/perspective_camera.h"
#include "../../math/matrix_util.h"

namespace apex {
Probe::Probe(const Vector3 &origin, int width, int height, float near, float far)
    : m_origin(origin),
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
    MatrixUtil::ToPerspective(m_proj_matrix, 90.0f, width, height, near, far);
}

void Probe::UpdateMatrices()
{
    for (int i = 0; i < m_matrices.size(); i++) {
        MatrixUtil::ToLookAt(m_matrices[i], m_origin, m_origin + m_directions[i].first, m_directions[i].second);

        m_matrices[i] = m_matrices[i] * m_proj_matrix;
    }
}

void Probe::Begin()
{
    UpdateMatrices();
}

void Probe::End()
{
}
} // namespace apex
