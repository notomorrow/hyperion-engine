#ifndef SKYDOME_H
#define SKYDOME_H

#include "../../entity.h"
#include "../../control.h"
#include "../mesh.h"
#include "../texture.h"
#include "../camera/camera.h"
#include "skydome_shader.h"
#include "clouds_shader.h"

#include <memory>

namespace apex {
class SkydomeControl : public EntityControl {
public:
    SkydomeControl(Camera *camera);

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    static const bool clouds_in_dome;

    std::shared_ptr<Entity> dome;
    std::shared_ptr<SkydomeShader> shader;
    std::shared_ptr<Mesh> clouds_quad;
    std::shared_ptr<CloudsShader> clouds_shader;
    Camera *camera;
    double global_time;
};
}

#endif