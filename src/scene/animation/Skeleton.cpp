/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/animation/Skeleton.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/animation/Animation.hpp>

#include <rendering/RenderSkeleton.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

Skeleton::Skeleton()
    : HypObject(),
      m_mutationState(DataMutationState::CLEAN)
{
}

Skeleton::Skeleton(const Handle<Bone>& rootBone)
    : HypObject(),
      m_rootBone(rootBone),
      m_mutationState(DataMutationState::CLEAN)
{
    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    if (m_renderResource != nullptr)
    {
        FreeResource(m_renderResource);

        m_renderResource = nullptr;
    }

    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(nullptr);
    }

    SetReady(false);
}

void Skeleton::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            if (m_renderResource != nullptr)
            {
                FreeResource(m_renderResource);

                m_renderResource = nullptr;
            }
        }));

    m_renderResource = AllocateResource<RenderSkeleton>(this);

    m_mutationState |= DataMutationState::DIRTY;

    Update(0.0166f);

    SetReady(true);
}

void Skeleton::Update(float)
{
    if (!m_mutationState.IsDirty())
    {
        return;
    }

    const SizeType numBones = MathUtil::Min(SkeletonShaderData::maxBones, NumBones());

    if (numBones != 0)
    {
        SkeletonShaderData shaderData {};
        shaderData.bones[0] = m_rootBone->GetBoneMatrix();

        for (SizeType i = 1; i < numBones; i++)
        {
            if (Node* descendent = m_rootBone->GetDescendants()[i - 1])
            {
                if (!descendent)
                {
                    continue;
                }

                if (descendent->GetType() != Node::Type::BONE)
                {
                    continue;
                }

                shaderData.bones[i] = static_cast<const Bone*>(descendent)->GetBoneMatrix();
            }
        }

        m_renderResource->SetBufferData(shaderData);
    }

    m_mutationState = DataMutationState::CLEAN;
}

Bone* Skeleton::FindBone(WeakName name) const
{
    if (!m_rootBone)
    {
        return nullptr;
    }

    if (m_rootBone->GetName() == name)
    {
        return static_cast<Bone*>(m_rootBone.Get());
    }

    for (Node* node : m_rootBone->GetDescendants())
    {
        if (!node)
        {
            continue;
        }

        if (node->GetType() != Node::Type::BONE)
        {
            continue;
        }

        Bone* bone = static_cast<Bone*>(node); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name)
        {
            return bone;
        }
    }

    return nullptr;
}

uint32 Skeleton::FindBoneIndex(WeakName name) const
{
    if (!m_rootBone)
    {
        return uint32(-1);
    }

    uint32 index = 0;

    if (m_rootBone->GetName() == name)
    {
        return index;
    }

    for (Node* node : m_rootBone->GetDescendants())
    {
        ++index;

        if (!node)
        {
            continue;
        }

        if (node->GetType() != Node::Type::BONE)
        {
            continue;
        }

        const Bone* bone = static_cast<const Bone*>(node); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name)
        {
            return index;
        }
    }

    return uint32(-1);
}

const Handle<Bone>& Skeleton::GetRootBone() const
{
    return m_rootBone;
}

void Skeleton::SetRootBone(const Handle<Bone>& bone)
{
    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(nullptr);

        m_rootBone.Reset();
    }

    if (!bone)
    {
        return;
    }

    m_rootBone = bone;
    m_rootBone->SetSkeleton(this);
}

SizeType Skeleton::NumBones() const
{
    if (!m_rootBone)
    {
        return 0;
    }

    return 1 + m_rootBone->GetDescendants().Size();
}

void Skeleton::AddAnimation(const Handle<Animation>& animation)
{
    if (!animation)
    {
        return;
    }

    for (const Handle<AnimationTrack>& track : animation->GetTracks())
    {
        if (!track->GetDesc().boneName.IsValid())
        {
            track->SetBone(nullptr);

            continue;
        }

        track->SetBone(FindBone(track->GetDesc().boneName));

        if (!track->GetBone())
        {
            HYP_LOG(Animation, Warning, "Skeleton could not find bone with name '{}'", track->GetDesc().boneName);
        }
    }

    m_animations.PushBack(animation);
}

const Animation* Skeleton::FindAnimation(UTF8StringView name, uint32* outIndex) const
{
    const auto it = m_animations.FindIf([&name](const auto& item)
        {
            return item->GetName() == name;
        });

    if (it == m_animations.End())
    {
        return nullptr;
    }

    if (outIndex != nullptr)
    {
        *outIndex = m_animations.IndexOf(it);
    }

    return it->Get();
}

} // namespace hyperion
