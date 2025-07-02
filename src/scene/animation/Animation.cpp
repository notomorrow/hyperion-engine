/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/animation/Animation.hpp>
#include <scene/animation/Bone.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

#pragma region AnimationTrack

AnimationTrack::AnimationTrack()
    : m_bone(nullptr)
{
}

AnimationTrack::AnimationTrack(const AnimationTrackDesc& desc)
    : m_bone(nullptr),
      m_desc(desc)
{
}

void AnimationTrack::Init()
{
    SetReady(true);
}

float AnimationTrack::GetLength() const
{
    if (m_desc.keyframes.Empty())
    {
        return 0.0f;
    }

    return m_desc.keyframes.Back().time;
}

Keyframe AnimationTrack::GetKeyframe(float time) const
{
    int first = 0, second = -1;

    if (m_desc.keyframes.Empty())
    {
        return { time, Transform() };
    }

    for (int i = 0; i < int(m_desc.keyframes.Size() - 1); i++)
    {
        if (MathUtil::InRange(time, { m_desc.keyframes[i].time, m_desc.keyframes[i + 1].time }))
        {
            first = i;
            second = i + 1;

            break;
        }
    }

    const Keyframe& current = m_desc.keyframes[first];

    Transform transform = current.transform;

    if (second > first)
    {
        const Keyframe& next = m_desc.keyframes[second];

        const float delta = (time - current.time) / (next.time - current.time);

        transform.translation = transform.translation.Lerp(next.transform.translation, delta);
        transform.rotation = transform.rotation.Slerp(next.transform.rotation, delta);
        transform.UpdateMatrix();
    }

    return { time, transform };
}

#pragma endregion AnimationTrack

#pragma region Animation

Animation::Animation() = default;

Animation::Animation(const String& name)
    : m_name(name)
{
}

void Animation::Init()
{
    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        InitObject(track);
    }

    SetReady(true);
}

void Animation::Apply(float time)
{
    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        if (track->m_bone == nullptr)
        {
            continue;
        }

        track->m_bone->ClearPose();
        track->m_bone->SetKeyframe(track->GetKeyframe(time));
    }
}

void Animation::ApplyBlended(float time, float blend)
{
    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        if (track->m_bone == nullptr)
        {
            continue;
        }

        if (blend <= MathUtil::epsilonF)
        {
            track->m_bone->ClearPose();
        }

        Keyframe frame = track->GetKeyframe(time);
        Keyframe blended = track->m_bone->GetKeyframe().Blend(
            frame,
            MathUtil::Clamp(blend, 0.0f, 1.0f));

        track->m_bone->SetKeyframe(blended);
    }
}

#pragma endregion Animation

} // namespace hyperion
