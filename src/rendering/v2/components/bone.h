#ifndef HYPERION_V2_BONE_H
#define HYPERION_V2_BONE_H

#include "node.h"
#include <math/transform.h>

namespace hyperion::v2 {

class Keyframe {
public:
    Keyframe();
    Keyframe(const Keyframe &other);
    Keyframe(float time, const Transform &transform);

    float GetTime() const { return m_time; }
    void SetTime(float time) { m_time = time; }
   
    const Transform &GetTransform() const { return m_transform; }
    void SetTransform(const Transform &transform) { m_transform = transform; }

    Keyframe Blend(const Keyframe &to, float blend) const;

private:
    float m_time;
    Transform m_transform;
};

class Bone : public Node {
public:
    Bone(const char *tag = "");
    Bone(const Bone &other) = delete;
    Bone &operator=(const Bone &other) = delete;
    ~Bone();

    Vector3 GetOffsetTranslation() const;
    Quaternion GetOffsetRotation() const;

    const Keyframe &GetKeyframe() const { return m_keyframe; }
    void SetKeyframe(const Keyframe &keyframe);

    void ClearPose();

    const Matrix4 &GetBoneMatrix() const { return m_bone_matrix; }

    void SetBindingTranslation(const Vector3 &translation) { m_binding_translation = translation; }
    void SetBindingRotation(const Quaternion &rotation) { m_binding_rotation = rotation; }

    void SetToBindingPose();
    void StoreBindingPose();

    void CalculateBoneTranslation();
    void CalculateBoneRotation();

    void UpdateBoneTransform();

    Vector3 m_world_bone_translation,
            m_binding_translation,
            m_inv_binding_translation,
            m_pose_translation;

    Quaternion m_world_bone_rotation,
               m_binding_rotation,
               m_inv_binding_rotation,
               m_pose_rotation;

private:

    Matrix4 m_bone_matrix;
    Keyframe m_keyframe;
};

} // namespace hyperion::v2

#endif