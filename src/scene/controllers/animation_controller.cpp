#include "animation_controller.h"
#include <set>
#include <iterator>

namespace hyperion::v2 {

AnimationController::AnimationController()
    : Controller("AnimationController"),
      m_state{}
{
}

void AnimationController::OnAdded()
{
    FindSkeleton(GetParent());
}

void AnimationController::OnRemoved()
{
    m_skeleton = nullptr;
}

void AnimationController::OnUpdate(GameCounter::TickUnit delta)
{
    if (IsPlaying()) {
        if (auto *animation = GetCurrentAnimation()) {
            m_state.current_time += delta * m_state.speed;

            if (m_state.current_time > animation->GetLength()) {
                m_state.current_time = 0.0f;

                if (m_state.loop_mode == LoopMode::ONCE) {
                    SetPlaybackState(PlaybackState::STOPPED);
                }
            }

            animation->ApplyBlended(m_state.current_time, 0.5f);
        } else {
            m_state = {};
        }
    }
}

bool AnimationController::FindSkeleton(Node *node)
{
    if (auto *spatial = node->GetSpatial()) {
        if (auto &skeleton = spatial->GetSkeleton()) {
            m_skeleton = skeleton.IncRef();

            return true;
        }
    }

    return std::any_of(
        node->GetChildren().begin(),
        node->GetChildren().end(),
        [this](const auto &child) {
            return child != nullptr && FindSkeleton(child.get());
        }
    );
}

void AnimationController::Play(LoopMode loop_mode)
{
    Play(m_state.speed, loop_mode);
}

void AnimationController::Play(float speed, LoopMode loop_mode)
{
    if (m_skeleton == nullptr || m_skeleton->NumAnimations() == 0) {
        Stop();

        return;
    }

    if (!MathUtil::InRange(m_state.animation_index, {0u, static_cast<uint32_t>(m_skeleton->NumAnimations())})) {
        m_state.animation_index = 0;
    }

    m_state.speed     = speed;
    m_state.loop_mode = loop_mode;

    SetPlaybackState(PlaybackState::PLAYING);
}

void AnimationController::Play(const std::string &animation_name, float speed, LoopMode loop_mode)
{
    if (m_skeleton == nullptr || m_skeleton->NumAnimations() == 0) {
        return;
    }

    size_t index;

    if (m_skeleton->FindAnimation(animation_name, &index) == nullptr) {
        Stop();

        return;
    }

    m_state.animation_index = static_cast<uint32_t>(index);
    m_state.speed           = speed;
    m_state.loop_mode       = loop_mode;

    SetPlaybackState(PlaybackState::PLAYING);
}

void AnimationController::Pause()
{
    SetPlaybackState(PlaybackState::PAUSED);    
}

void AnimationController::Stop()
{
    m_state = {};

    SetPlaybackState(PlaybackState::STOPPED);
}

}