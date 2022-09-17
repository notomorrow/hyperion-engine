#ifndef HYPERION_V2_RENDER_STATE_HPP
#define HYPERION_V2_RENDER_STATE_HPP

#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/DrawProxy.hpp>
#include <scene/Scene.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>

#include <stack>

namespace hyperion::v2 {

class RenderEnvironment;

struct RenderState
{
    struct SceneBinding
    {
        static const SceneBinding empty;

        Scene::ID id;
        RenderEnvironment *render_environment = nullptr;
        CameraDrawProxy camera;

        explicit operator bool() const { return bool(id); }
    };

    std::stack<SceneBinding> scene_bindings;
    FlatSet<Light::ID> light_ids;
    FlatMap<EnvProbe::ID, Optional<UInt>> env_probes; // map to texture slot
    UInt8 visibility_cursor = 0u;

    void BindLight(Light::ID light)
    {
        light_ids.Insert(light);
    }

    void UnbindLight(Light::ID light)
    {
        light_ids.Erase(light);
    }

    void BindScene(const Scene *scene)
    {
        if (scene == nullptr) {
            scene_bindings.push(SceneBinding::empty);
        } else {
            scene_bindings.push(SceneBinding {
                scene->GetID(),
                scene->GetEnvironment(),
                scene->GetCamera()
                    ? scene->GetCamera()->GetDrawProxy()
                    : CameraDrawProxy { }
            });
        }
    }

    void UnbindScene()
    {
        scene_bindings.pop();
    }

    const SceneBinding &GetScene() const
    {
        return scene_bindings.empty()
            ? SceneBinding::empty
            : scene_bindings.top();
    }

    void BindEnvProbe(EnvProbe::ID env_probe)
    {
        if (m_env_probe_texture_slot_counter >= max_bound_env_probes) {
            DebugLog(
                LogType::Warn,
                "Maximum bound env probes (%u) exceeded!\n",
                max_bound_env_probes
            );

            return;
        }

        env_probes.Insert(env_probe, Optional<UInt>(m_env_probe_texture_slot_counter++));
    }

    void Reset()
    {
        env_probes.Clear();
        m_env_probe_texture_slot_counter = 0u;

        scene_bindings = {};

        light_ids.Clear();

        visibility_cursor = 0u;
    }

private:
    UInt m_env_probe_texture_slot_counter = 0u;
};

} // namespace hyperion::v2

#endif