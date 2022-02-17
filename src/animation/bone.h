#ifndef BONE_H
#define BONE_H

#include "../scene/node.h"
#include "keyframe.h"

#include <string>

namespace hyperion {
class Bone : public Node {
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

    inline const Vector3 &GetPoseTranslation() const { return pose_pos; }
    inline void SetPoseTranslation(const Vector3 &translation)
    {
        pose_pos = translation;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Quaternion &GetPoseRotation() const { return pose_rot; }
    inline void SetPoseRotation(const Quaternion &rotation)
    {
        pose_rot = rotation;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    const Matrix4 &GetBoneMatrix() const;

    void UpdateTransform() override;

    virtual std::shared_ptr<Loadable> Clone() override;

    Vector3 global_bone_pos,
        bind_pos,
        inv_bind_pos,
        pose_pos;

    Quaternion global_bone_rot,
        bind_rot,
        inv_bind_rot,
        pose_rot;

private:
    Matrix4 bone_matrix;
    Keyframe current_pose;

    std::shared_ptr<Bone> CloneImpl();

    inline Vector3 GetOffsetTranslation() const { return m_local_translation - bind_pos; }
    inline Quaternion GetOffsetRotation() const
    {
        Quaternion inv_bind_rot_local(bind_rot);
        inv_bind_rot_local.Invert();
        return m_local_rotation * inv_bind_rot_local;
    }
};
}

#endif
