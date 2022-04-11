#include "bone.h"

namespace hyperion::v2 {

Keyframe::Keyframe()
    : m_time(0.0f)
{
}

Keyframe::Keyframe(const Keyframe &other)
    : m_time(other.m_time),
      m_transform(other.m_transform)
{
}

Keyframe::Keyframe(float time, const Transform &transform)
    : m_time(time),
      m_transform(transform)
{
}

Bone::Bone(const char *tag)
    : Node(Type::BONE, tag, nullptr, Transform())
{
}

Bone::~Bone() = default;

Vector3 Bone::GetOffsetTranslation() const
{
    return m_local_transform.GetTranslation() - m_binding_translation;
}

Quaternion Bone::GetOffsetRotation() const
{
    return m_local_transform.GetRotation() * Quaternion(m_binding_rotation).Invert();
}

void Bone::SetKeyframe(const Keyframe &keyframe)
{
    m_keyframe = keyframe;

    m_pose_translation = m_keyframe.GetTransform().GetTranslation();
    m_pose_rotation = m_keyframe.GetTransform().GetRotation();

    UpdateBoneTransform();
}

void Bone::ClearPose()
{
    m_pose_translation = Vector3();
    m_pose_rotation = Quaternion();

    UpdateBoneTransform();

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->ClearPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::StoreBindingPose()
{
    m_inv_binding_translation = m_world_bone_translation * -1.0f;
    m_inv_binding_rotation = Quaternion(m_world_bone_rotation).Invert();

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->StoreBindingPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::SetToBindingPose()
{
    m_local_transform.SetTranslation(m_binding_translation);
    m_local_transform.SetRotation(m_binding_rotation);

    m_pose_translation = m_binding_translation;
    m_pose_rotation = m_binding_rotation;

    UpdateBoneTransform();

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->SetToBindingPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneTranslation()
{
    m_world_bone_translation = m_binding_translation;

    if (m_parent_node != nullptr && m_parent_node->GetType() == Type::BONE) {
        auto *parent_bone = static_cast<Bone *>(m_parent_node);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_world_bone_translation = parent_bone->m_world_bone_rotation * m_binding_translation;
        m_world_bone_translation += parent_bone->m_world_bone_translation;
    }

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->CalculateBoneTranslation();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneRotation()
{
    m_world_bone_rotation = m_binding_rotation;

    if (m_parent_node != nullptr && m_parent_node->GetType() == Type::BONE) {
        auto *parent_bone = static_cast<Bone *>(m_parent_node);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_world_bone_rotation = parent_bone->m_world_bone_rotation * m_binding_rotation;
    }

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->CalculateBoneRotation();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::UpdateBoneTransform()
{
    Matrix4 tmp;

    Vector3 inv_translation = m_world_bone_translation * -1.0f;
    
    MatrixUtil::ToTranslation(m_bone_matrix, inv_translation);
    MatrixUtil::ToRotation(tmp, m_world_bone_rotation * m_pose_rotation * GetOffsetRotation() * m_inv_binding_rotation);
    m_bone_matrix *= tmp;

    inv_translation *= -1.0f;
    MatrixUtil::ToTranslation(tmp, inv_translation);
    m_bone_matrix *= tmp;

    MatrixUtil::ToTranslation(tmp, m_pose_translation);
    m_bone_matrix *= tmp;

    MatrixUtil::ToTranslation(tmp, GetOffsetTranslation());
    m_bone_matrix *= tmp;

    if (m_parent_node != nullptr) {
        if (m_parent_node->GetType() == Type::BONE) {
            m_bone_matrix *= static_cast<Bone *>(m_parent_node)->GetBoneMatrix();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }

        m_world_transform = m_local_transform * m_parent_node->GetWorldTransform();
    }

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->UpdateBoneTransform();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

} // namespace hyperion::v2