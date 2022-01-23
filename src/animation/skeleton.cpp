#include "skeleton.h"

namespace hyperion {
Skeleton::Skeleton(const std::string &name)
    : m_name(name)
{
}

std::shared_ptr<Animation> Skeleton::GetAnimation(const std::string &name)
{
    for (auto &anim : animations) {
        if (anim->GetName() == name) {
            return anim;
        }
    }
    return nullptr;
}

std::shared_ptr<Animation> Skeleton::GetAnimation(size_t index)
{
    return animations[index];
}

void Skeleton::AddAnimation(std::shared_ptr<Animation> anim)
{
    animations.push_back(anim);
}

std::shared_ptr<Bone> Skeleton::GetBone(const std::string &name)
{
    for (auto &&bone : bones) {
        if (bone->GetName() == name) {
            return bone;
        }
    }
    return nullptr;
}

std::shared_ptr<Bone> Skeleton::GetBone(size_t index)
{
    return bones.at(index);
}

void Skeleton::AddBone(const std::shared_ptr<Bone> bone)
{
    bones.push_back(bone);
}

std::shared_ptr<Loadable> Skeleton::Clone()
{
    std::shared_ptr<Skeleton> new_skeleton = std::make_shared<Skeleton>(m_name);

    for (auto &bone : bones) {
        new_skeleton->bones.push_back(bone->CloneImpl());
    }

    return new_skeleton;
}
}
