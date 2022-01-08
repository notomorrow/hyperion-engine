#include "bone.h"
#include "../math/matrix_util.h"

namespace apex {
Bone::Bone(const std::string &name)
    : Entity(name)
{
}

void Bone::ClearPose()
{
    pose_pos = Vector3::Zero();
    pose_rot = Quaternion::Identity();

    SetTransformUpdateFlag();
}

void Bone::ApplyPose(const Keyframe &pose)
{
    current_pose = pose;

    pose_pos = pose.GetTranslation();
    pose_rot = pose.GetRotation();

    SetTransformUpdateFlag();
}

const Keyframe &Bone::GetCurrentPose() const
{
    return current_pose;
}

void Bone::StoreBindingPose()
{
    inv_bind_pos = global_bone_pos * -1;
    inv_bind_rot = global_bone_rot;
    inv_bind_rot.Invert();
}

void Bone::SetToBindingPose()
{
    m_local_rotation = bind_rot;
    std::cout << GetName() << " reset rotation to " << m_local_rotation << "\n";
    m_local_translation = bind_pos;

    pose_pos = bind_pos;
    pose_rot = bind_rot;

    SetTransformUpdateFlag();
}

const Vector3 &Bone::CalcBindingTranslation()
{
    global_bone_pos = bind_pos;

    Bone *parent_bone;
    if (m_parent && (parent_bone = dynamic_cast<Bone*>(m_parent))) {
        global_bone_pos = parent_bone->global_bone_rot * bind_pos;
        global_bone_pos += parent_bone->global_bone_pos;
    }

    for (auto &child : m_children) {
        auto child_bone = std::dynamic_pointer_cast<Bone>(child);
        if (child_bone) {
            child_bone->CalcBindingTranslation();
        }
    }

    return global_bone_pos;
}

const Quaternion &Bone::CalcBindingRotation()
{
    global_bone_rot = bind_rot;

    Bone *parent_bone;
    if (m_parent && (parent_bone = dynamic_cast<Bone*>(m_parent))) {
        global_bone_rot = parent_bone->global_bone_rot * bind_rot;
    }

    for (auto &&child : m_children) {
        auto child_bone = std::dynamic_pointer_cast<Bone>(child);
        if (child_bone) {
            child_bone->CalcBindingRotation();
        }
    }

    return global_bone_rot;
}

const Matrix4 &Bone::GetBoneMatrix() const
{
    return bone_matrix;
}

void Bone::UpdateTransform()
{
    Matrix4 scale_matrix, tmp_scale;

    if (m_parent != nullptr) {
        MatrixUtil::ToScaling(tmp_scale, m_parent->GetGlobalTransform().GetScale());
        scale_matrix *= tmp_scale;
    }

    MatrixUtil::ToScaling(tmp_scale, m_local_scale);
    scale_matrix *= tmp_scale;
 
    Vector3 tmp_pos = (global_bone_pos) * -1;

    Matrix4 rot_matrix;
    MatrixUtil::ToTranslation(rot_matrix, tmp_pos);

    Quaternion tmp_rot = global_bone_rot * pose_rot * GetOffsetRotation() * inv_bind_rot;

    std::cout << GetName() << " bind_rot = " << bind_rot << "\n";
    std::cout << GetName() << " GetOffsetRotation() = " << GetOffsetRotation() << "\n";

    Matrix4 tmp_matrix;
    MatrixUtil::ToRotation(tmp_matrix, tmp_rot);
    rot_matrix *= tmp_matrix;

    tmp_pos *= -1;
    MatrixUtil::ToTranslation(tmp_matrix, tmp_pos);
    rot_matrix *= tmp_matrix;

    MatrixUtil::ToTranslation(tmp_matrix, pose_pos);
    rot_matrix *= tmp_matrix;

    MatrixUtil::ToTranslation(tmp_matrix, GetOffsetTranslation());
    rot_matrix *= tmp_matrix;

    bone_matrix = rot_matrix;

    Bone *parent_bone;
    if (m_parent != nullptr) {
        parent_bone = dynamic_cast<Bone*>(m_parent);
        if (parent_bone != nullptr) {
            bone_matrix *= parent_bone->bone_matrix;
        }
    }

    Entity::UpdateTransform();
}

std::shared_ptr<Loadable> Bone::Clone()
{
    return CloneImpl();
}

std::shared_ptr<Bone> Bone::CloneImpl()
{
    std::shared_ptr<Bone> new_bone = std::make_shared<Bone>(m_name);

    new_bone->global_bone_pos = global_bone_pos;
    new_bone->bind_pos = bind_pos;
    new_bone->inv_bind_pos = inv_bind_pos;
    new_bone->pose_pos = pose_pos;
    new_bone->m_local_translation = m_local_translation;

    new_bone->global_bone_rot = global_bone_rot;
    new_bone->bind_rot = bind_rot;
    new_bone->inv_bind_rot = inv_bind_rot;
    new_bone->pose_rot = pose_rot;
    new_bone->m_local_rotation = m_local_rotation;

    new_bone->m_local_scale = m_local_scale;

    new_bone->bone_matrix = bone_matrix;
    new_bone->current_pose = current_pose;

    return new_bone;
}
}
