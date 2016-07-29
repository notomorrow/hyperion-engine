#ifndef BONE_H
#define BONE_H

#include "../entity.h"
#include "keyframe.h"

#include <string>

namespace apex {
class Bone : public Entity {
public:
    Bone(const std::string &name = "");

    void ClearPose();
    void ApplyPose(const Keyframe &pose);
    const Keyframe &GetCurrentPose() const;

    void StoreBindingPose();
    void SetToBindingPose();
    const Vector3 &CalcBindingTranslation();
    const Quaternion &CalcBindingRotation();

    const Matrix4 &GetBoneMatrix() const;

    void UpdateTransform() override;

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
};
}

#endif