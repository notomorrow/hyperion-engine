#ifndef BONE_H
#define BONE_H

#include "../entity.h"
#include "keyframe.h"

#include <string>

namespace apex {
class Bone : public Entity {
    friend class Skeleton;
public:
    Bone(const std::string &name = "");
    virtual ~Bone() = default;

    void ClearPose();
    void ApplyPose(const Keyframe &pose);
    const Keyframe &GetCurrentPose() const;

    void StoreBindingPose();
    void SetToBindingPose();
    const Vector3 &CalcBindingTranslation();
    const Quaternion &CalcBindingRotation();

    inline Transform &GetOffsetTransform() { return m_offset_transform; }
    inline const Transform &GetOffsetTransform() const { return m_offset_transform; }
    inline void SetOffsetTransform(const Transform &transform)
    {
        m_offset_transform = transform;
        SetAABBUpdateFlag();
    }

    inline const Vector3 &GetOffsetTranslation() const { return m_offset_transform.GetTranslation(); }
    inline void SetOffsetTranslation(const Vector3 &translation)
    {
        m_offset_transform.SetTranslation(translation);
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Vector3 &GetOffsetScale() const { return m_offset_transform.GetScale(); }
    inline void SetOffsetScale(const Vector3 &scale)
    {
        m_offset_transform.SetScale(scale);
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Quaternion &GetOffsetRotation() const { return m_offset_transform.GetRotation(); }
    inline void SetOffsetRotation(const Quaternion &scale)
    {
        m_offset_transform.SetRotation(scale);
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    const Matrix4 &GetBoneMatrix() const;

    void UpdateTransform() override;

    virtual std::shared_ptr<Loadable> Clone() override;

    Vector3 global_bone_pos,
        bind_pos,
        inv_bind_pos,
        pose_pos,
        user_pos;

    Quaternion global_bone_rot,
        bind_rot,
        inv_bind_rot,
        pose_rot,
        user_rot;

private:
    Matrix4 bone_matrix;
    Keyframe current_pose;

    Transform m_offset_transform;

    std::shared_ptr<Bone> CloneImpl();
};
}

#endif
