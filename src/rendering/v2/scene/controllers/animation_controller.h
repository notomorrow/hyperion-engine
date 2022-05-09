#ifndef HYPERION_V2_ANIMATION_CONTROLLER_H
#define HYPERION_V2_ANIMATION_CONTROLLER_H

#include "../controller.h"
#include "../../animation/bone.h"
#include "../../animation/animation.h"

namespace hyperion::v2 {

class AnimationController : public Controller {
public:
    enum class PlaybackState {
        STOPPED,
        PAUSED,
        PLAYING
    };

    enum class LoopMode {
        ONCE,
        REPEAT
    };

    AnimationController();
    virtual ~AnimationController() override = default;

    bool IsPlaying() const { return m_state.playback_state == PlaybackState::PLAYING; }
    bool IsPaused() const  { return m_state.playback_state == PlaybackState::PAUSED; }
    bool IsStopped() const { return m_state.playback_state == PlaybackState::STOPPED; }
    
    void Play(LoopMode loop_mode = LoopMode::ONCE);
    void Play(float speed, LoopMode loop_mode = LoopMode::ONCE);
    void Play(const std::string &animation_name, float speed, LoopMode loop_mode = LoopMode::ONCE);
    void Pause();
    void Stop();

    LoopMode GetLoopMode() const         { return m_state.loop_mode; }
    void SetLoopMode(LoopMode loop_mode) { m_state.loop_mode = loop_mode; }

    Animation *GetCurrentAnimation() const
    {
        return m_skeleton != nullptr && m_state.animation_index != ~0u
            ? m_skeleton->GetAnimation(m_state.animation_index)
            : nullptr;
    }

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void OnDescendentAdded(Node *) override {} /* TODO: check for bones, add them */
    virtual void OnDescendentRemoved(Node *node) override {} /* TODO: remove any bones */

private:
    bool FindSkeleton(Node *node);

    PlaybackState GetPlaybackState() const              { return m_state.playback_state; }
    void SetPlaybackState(PlaybackState playback_state) { m_state.playback_state = playback_state; }

    struct {
        PlaybackState playback_state = PlaybackState::STOPPED;
        LoopMode loop_mode           = LoopMode::ONCE;
        uint32_t animation_index     = ~0u;
        float  speed                 = 1.0f;
        float  current_time          = 0.0f;
    } m_state;

    Ref<Skeleton> m_skeleton;
    //std::vector<Bone *>    m_bones;

};

} // namespace hyperion::v2

#endif