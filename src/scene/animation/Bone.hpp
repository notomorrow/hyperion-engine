/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BONE_HPP
#define HYPERION_BONE_HPP

#include <scene/Node.hpp>
#include <scene/animation/Keyframe.hpp>

#include <core/containers/String.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Transform.hpp>

namespace hyperion {

class Skeleton;

HYP_CLASS()
class HYP_API Bone : public Node
{
    friend class Skeleton;

    HYP_OBJECT_BODY(Bone);

public:
    Bone();
    Bone(Name name);
    Bone(const Bone& other) = delete;
    Bone& operator=(const Bone& other) = delete;
    virtual ~Bone() override;

    Vec3f GetOffsetTranslation() const;
    Quaternion GetOffsetRotation() const;

    const Keyframe& GetKeyframe() const
    {
        return m_keyframe;
    }

    void SetKeyframe(const Keyframe& keyframe);

    void ClearPose();

    HYP_FORCE_INLINE const Matrix4& GetBoneMatrix() const
    {
        return m_boneMatrix;
    }

    HYP_FORCE_INLINE void SetBindingTransform(const Transform& transform)
    {
        m_bindingTransform = transform;
    }

    void SetToBindingPose();
    void StoreBindingPose();

    void CalculateBoneTranslation();
    void CalculateBoneRotation();

    void UpdateBoneTransform();

    Transform m_bindingTransform;
    Transform m_poseTransform;

    Vec3f m_worldBoneTranslation;
    Vec3f m_invBindingTranslation;

    Quaternion m_worldBoneRotation;
    Quaternion m_invBindingRotation;

private:
    void SetSkeleton(Skeleton* skeleton);

    Skeleton* GetSkeleton() const
    {
        return m_skeleton;
    }

    Skeleton* m_skeleton;
    Matrix4 m_boneMatrix;
    Keyframe m_keyframe;
};

} // namespace hyperion

#endif