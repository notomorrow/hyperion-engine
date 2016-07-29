#ifndef SHADOW_MAPPING_H
#define SHADOW_MAPPING_H

#include "../texture.h"
#include "../framebuffer.h"
#include "../camera/ortho_camera.h"

namespace apex {
class ShadowMapping {
public:
    ShadowMapping();
    ~ShadowMapping();

    OrthoCamera *GetShadowCamera();
    std::shared_ptr<Texture> GetShadowMap();

    void Begin();
    void End();

private:
    OrthoCamera *shadow_cam;
    Framebuffer *fbo;
};
}

#endif