/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/AnimationSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/animation/Animation.hpp>
#include <scene/animation/Skeleton.hpp>

#include <core/Handle.hpp>

namespace hyperion {

void AnimationSystem::OnEntityAdded(Entity* entity)
{
    const MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);
    InitObject(meshComponent.skeleton);
}

void AnimationSystem::Process(float delta)
{
    for (auto [entity, animationComponent, meshComponent] : GetEntityManager().GetEntitySet<AnimationComponent, MeshComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!meshComponent.skeleton)
        {
            continue;
        }

        AnimationPlaybackState& playbackState = animationComponent.playbackState;

        if (playbackState.status == AnimationPlaybackStatus::PLAYING)
        {
            if (playbackState.animationIndex == ~0u)
            {
                playbackState = {};

                continue;
            }

            const Handle<Animation>& animation = meshComponent.skeleton->GetAnimation(playbackState.animationIndex);
            Assert(animation.IsValid());

            playbackState.currentTime += delta * playbackState.speed;

            if (playbackState.currentTime > animation->GetLength())
            {
                playbackState.currentTime = 0.0f;

                if (playbackState.loopMode == AnimationLoopMode::ONCE)
                {
                    playbackState.status = AnimationPlaybackStatus::STOPPED;
                    playbackState.currentTime = 0.0f;
                }
            }

            animation->ApplyBlended(playbackState.currentTime, 0.5f);
        }

        meshComponent.skeleton->Update(delta);
    }
}

} // namespace hyperion