#ifndef SKELETON_CONTROL_H
#define SKELETON_CONTROL_H

#include "../control.h"
#include "../entity.h"
#include "../rendering/shader.h"
#include "bone.h"
#include "animation.h"

#include <memory>
#include <vector>
#include <string>

namespace apex {
class SkeletonControl : public EntityControl {
public:
    SkeletonControl(std::shared_ptr<Shader> skinning_shader);
    virtual ~SkeletonControl() = default;

    Bone *GetBone(size_t index);
    Bone *GetBone(const std::string &name);

    std::shared_ptr<Animation> GetAnimation(const std::string &name);
    std::shared_ptr<Animation> GetAnimation(size_t index);
    void AddAnimation(std::shared_ptr<Animation>);
    size_t NumAnimations() const;

    void PlayAnimation(int index, double speed = 1.0);
    void PlayAnimation(const std::string &name, double speed = 1.0);
    void PauseAnimation();
    void StopAnimation();

    void SetLoop(bool loop);

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    void FindBones(Entity *top);

    std::vector<std::string> bone_names;
    std::vector<Bone*> bones;
    std::vector<std::shared_ptr<Animation>> animations;
    std::shared_ptr<Shader> skinning_shader;

    Animation *current_anim;
    double play_speed, time;

    enum {
        STOPPED,
        PAUSED,
        PLAYING
    } play_state;

    enum {
        PLAY_ONCE,
        LOOP
    } loop_mode;
};
}

#endif