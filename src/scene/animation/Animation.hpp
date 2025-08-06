/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/animation/Keyframe.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <core/object/Handle.hpp>
#include <core/Name.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Bone;

struct AnimationTrackDesc
{
    Name boneName;
    Array<Keyframe> keyframes;
};

HYP_CLASS()
class AnimationTrack final : public HypObjectBase
{
    HYP_OBJECT_BODY(AnimationTrack);

public:
    friend class Bone;
    friend class Animation;

    HYP_API AnimationTrack();
    HYP_API AnimationTrack(const AnimationTrackDesc& desc);
    AnimationTrack(const AnimationTrack& other) = delete;
    AnimationTrack& operator=(const AnimationTrack& other) = delete;
    ~AnimationTrack() override = default;

    HYP_FORCE_INLINE Bone* GetBone() const
    {
        return m_bone;
    }

    /*! \internal Used by Skeleton class to set up mapping between bones and animation tracks. */
    HYP_FORCE_INLINE void SetBone(Bone* bone)
    {
        m_bone = bone;
    }

    HYP_FORCE_INLINE const AnimationTrackDesc& GetDesc() const
    {
        return m_desc;
    }

    HYP_METHOD()
    float GetLength() const;

    HYP_METHOD()
    HYP_API Keyframe GetKeyframe(float time) const;

private:
    void Init() override;

    mutable Bone* m_bone;
    AnimationTrackDesc m_desc;
};

HYP_CLASS()
class Animation final : public HypObjectBase
{
    HYP_OBJECT_BODY(Animation);

public:
    HYP_API Animation();
    HYP_API Animation(const String& name);
    Animation(const Animation& other) = delete;
    Animation& operator=(const Animation& other) = delete;
    ~Animation() = default;

    HYP_METHOD(Property = "Name", Serialize = true)
    const String& GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name", Serialize = true)
    void SetName(const String& name)
    {
        m_name = name;
    }

    HYP_METHOD(Property = "Length", Serialize = false)
    float GetLength() const
    {
        return m_tracks.Empty() ? 0.0f : m_tracks.Back()->GetLength();
    }

    HYP_METHOD()
    void AddTrack(const Handle<AnimationTrack>& track)
    {
        m_tracks.PushBack(track);
    }

    HYP_METHOD(Property = "Tracks", Serialize = true)
    const Array<Handle<AnimationTrack>>& GetTracks() const
    {
        return m_tracks;
    }

    HYP_METHOD(Property = "Tracks", Serialize = true)
    void SetTracks(const Array<Handle<AnimationTrack>>& tracks)
    {
        m_tracks = tracks;
    }

    HYP_METHOD()
    const Handle<AnimationTrack>& GetTrack(uint32 index) const
    {
        return m_tracks[index];
    }

    HYP_METHOD()
    uint32 NumTracks() const
    {
        return uint32(m_tracks.Size());
    }

    HYP_METHOD()
    HYP_API void Apply(float time);

    HYP_METHOD()
    HYP_API void ApplyBlended(float time, float blend);

private:
    void Init() override;

    String m_name;
    Array<Handle<AnimationTrack>> m_tracks;
};

} // namespace hyperion

