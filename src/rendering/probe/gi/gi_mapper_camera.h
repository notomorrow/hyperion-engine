#ifndef GI_MAPPER_CAMERA_H
#define GI_MAPPER_CAMERA_H

#include "../probe_camera.h"
#include "../probe_region.h"
#include "../../../math/bounding_box.h"
#include "../../texture_3D.h"
#include "../../renderable.h"

#include <memory>

namespace hyperion {
class Shader;
class ComputeShader;

class GIMapperCamera : public ProbeCamera {
public:
    GIMapperCamera(const ProbeRegion &region);
    virtual ~GIMapperCamera();

    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;
};
} // namespace hyperion

#endif