#include "skydome.h"
#include "../shader_manager.h"
#include "../../util/mesh_factory.h"
#include "../../asset/asset_manager.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace apex {
const bool SkydomeControl::clouds_in_dome = false;

SkydomeControl::SkydomeControl(Camera *camera)
    : EntityControl(25.0), camera(camera), global_time(0.0)
{
}

void SkydomeControl::OnAdded()
{
    ShaderProperties defines { {"CLOUDS", clouds_in_dome} };
    shader = ShaderManager::GetInstance()->GetShader<SkydomeShader>(defines);

    dome = AssetManager::GetInstance()->LoadFromFile<Entity>("res\\models\\skydome\\dome.obj");
    if (dome == nullptr) {
        throw std::runtime_error("Could not load skydome model!");
    }
    dome->SetLocalScale(30);
    dome->GetChild(0)->GetRenderable()->SetShader(shader);
    dome->GetChild(0)->GetRenderable()->SetRenderBucket(Renderable::RB_SKY);
    dome->GetChild(0)->GetRenderable()->GetMaterial().SetParameter("DepthWriteOff", 1);
    dome->GetChild(0)->GetRenderable()->GetMaterial().SetParameter("BlendMode", 1/* alpha blended */);

    if (!clouds_in_dome) {
        ShaderProperties clouds_defines;
        clouds_shader = ShaderManager::GetInstance()->GetShader<CloudsShader>(clouds_defines);
        clouds_shader->SetCloudColor(Vector4(1.0));

        clouds_quad = MeshFactory::CreateQuad();
        clouds_quad->SetShader(clouds_shader);
        clouds_quad->SetRenderBucket(Renderable::RB_SKY);
        clouds_quad->GetMaterial().SetParameter("DepthWriteOff", 1);
        clouds_quad->GetMaterial().SetParameter("BlendMode", 1/* alpha blended */);

        auto clouds_node = std::make_shared<Entity>("clouds");
        clouds_node->Move(Vector3::UnitY() * 12);
        clouds_node->Rotate(Quaternion(Vector3::UnitX(), M_PI / 2));
        clouds_node->SetRenderable(clouds_quad);
        dome->AddChild(clouds_node);
    }

    parent->AddChild(dome);
}

void SkydomeControl::OnRemoved()
{
    parent->RemoveChild(dome);
}

void SkydomeControl::OnUpdate(double dt)
{
    global_time += 0.01;
    shader->SetGlobalTime(global_time);
    shader->SetCameraHeight(camera->GetHeight());
    shader->SetCameraPosition(camera->GetTranslation());
    dome->SetLocalTranslation(camera->GetTranslation());

    if (!clouds_in_dome) {
        clouds_shader->SetGlobalTime(global_time);
    }
}
}