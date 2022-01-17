#include "skybox.h"
#include "skybox_shader.h"
#include "../../entity.h"
#include "../shader_manager.h"
#include "../environment.h"
#include "../../util/mesh_factory.h"
#include "../../asset/asset_manager.h"
#include "../../math/math_util.h"

namespace apex {
SkyboxControl::SkyboxControl(Camera *camera, const std::shared_ptr<Cubemap> &cubemap)
    : EntityControl(),
      camera(camera),
      cubemap(cubemap)
{
}

void SkyboxControl::OnAdded()
{
    cube = std::make_shared<Entity>("Skybox");

    cube->SetRenderable(MeshFactory::CreateCube());
    cube->SetLocalScale(10);
    cube->SetLocalTranslation(Vector3(0, 55, 2));
    cube->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<SkyboxShader>(ShaderProperties()));
    cube->GetRenderable()->SetRenderBucket(Renderable::RB_SKY);
    cube->GetMaterial().SetTexture("SkyboxMap", cubemap);
    cube->GetMaterial().depth_test = false;
    cube->GetMaterial().depth_write = false;
    cube->GetMaterial().alpha_blended = true;

    parent->AddChild(cube);
}

void SkyboxControl::OnRemoved()
{
    parent->RemoveChild(cube);
}

void SkyboxControl::OnUpdate(double dt)
{
}
} // namespace apex
