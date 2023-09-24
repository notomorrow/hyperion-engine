#include <scene/camera/CameraTrack.hpp>
#include <math/MathUtil.hpp>

namespace hyperion::v2 {

CameraTrack::CameraTrack(Double duration)
    : m_duration(duration)
{
}

CameraTrackPivot CameraTrack::GetPivotAt(Double timestamp) const
{
    const Double fraction = timestamp / MathUtil::Max(m_duration, 0.00001);

    Int first = 0, second = -1;

    if (m_pivots.Empty()) {
        return CameraTrackPivot { fraction };
    }

    for (Int i = 0; i < Int(m_pivots.Size() - 1); i++) {
        if (MathUtil::InRange(fraction, { m_pivots[i].fraction, m_pivots[i + 1].fraction })) {
            first = i;
            second = i + 1;

            break;
        }
    }

    const CameraTrackPivot &current = m_pivots[first];

    Transform transform = current.transform;

    if (second > first) {
        const CameraTrackPivot &next = m_pivots[second];

        const Double delta = (fraction - current.fraction) / (next.fraction - current.fraction);

        transform.GetTranslation().Lerp(next.transform.GetTranslation(), Float(delta));
        transform.GetRotation().Slerp(next.transform.GetRotation(), Float(delta));
        transform.UpdateMatrix();
    }

    return { fraction, transform };
}

void CameraTrack::AddPivot(const CameraTrackPivot &pivot)
{
    m_pivots.Insert(pivot);
}

} // namespace hyperion::v2