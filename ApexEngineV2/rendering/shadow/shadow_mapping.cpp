#include "shadow_mapping.h"

namespace apex {
ShadowMapping::ShadowMapping()
{
    shadow_cam = new OrthoCamera(-12, 12, -12, 12, -25, 25);
    fbo = new Framebuffer(512, 512);
    shadow_cam->SetTranslation(Vector3(-11, 11, -1));
    shadow_cam->SetDirection(Vector3(0.5, -0.5, 0.5));
}

ShadowMapping::~ShadowMapping()
{
    delete fbo;
    delete shadow_cam;
}

OrthoCamera *ShadowMapping::GetShadowCamera()
{
    return shadow_cam;
}

std::shared_ptr<Texture> ShadowMapping::GetShadowMap()
{
    return fbo->GetDepthTexture();
}

void ShadowMapping::Begin()
{
    fbo->Use();
}

void ShadowMapping::End()
{
    fbo->End();
}
}