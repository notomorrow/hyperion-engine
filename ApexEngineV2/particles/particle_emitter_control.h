#ifndef PARTICLE_EMITTER_CONTROL_H
#define PARTICLE_EMITTER_CONTROL_H

#include "../control.h"
#include "../rendering/camera/camera.h"
#include "particle.h"
#include "particle_construction_info.h"
#include "particle_renderer.h"

#include <vector>
#include <memory>

namespace apex {
class ParticleEmitterControl : public EntityControl {
public:
    ParticleEmitterControl(Camera *camera);

    void ResetParticle(Particle &particle);

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    Camera *m_camera;
    std::vector<Particle> m_particles;
    ParticleRenderer *m_renderer;
};
} // namespace apex

#endif