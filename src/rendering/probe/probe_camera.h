#ifndef PROBE_CAMERA_H
#define PROBE_CAMERA_H

#include "probe_region.h"
#include "../renderable.h"

#include <memory>

namespace hyperion {
class ProbeCamera : public Renderable {
public:
    ProbeCamera(const ProbeRegion &region, int width, int height, float near, float far);
    ProbeCamera(const ProbeCamera &other) = delete;
    ProbeCamera &operator=(const ProbeCamera &other) = delete;
    ~ProbeCamera();

    inline ProbeRegion &GetRegion() { return m_region; }
    inline const ProbeRegion &GetRegion() const { return m_region; }
    inline Camera *GetCamera() { return m_camera; }
    inline const Camera *GetCamera() const { return m_camera; }

    void Update(double dt);

    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    ProbeRegion m_region;
    Camera *m_camera;
};
} // namespace hyperion

#endif
