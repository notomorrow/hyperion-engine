#ifndef LIGHT_VOLUME_PROBE_CAMERA_H
#define LIGHT_VOLUME_PROBE_CAMERA_H

#include "../probe_camera.h"

#include <memory>

namespace hyperion {
class LightVolumeProbeCamera : public ProbeCamera {
public:
    LightVolumeProbeCamera(const ProbeRegion &region, int width, int height, float near, float far);
    LightVolumeProbeCamera(const LightVolumeProbeCamera &other) = delete;
    LightVolumeProbeCamera &operator=(const LightVolumeProbeCamera &other) = delete;
    ~LightVolumeProbeCamera();

    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;
};
} // namespace hyperion

#endif
