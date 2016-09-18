#include "skydome.h"
#include "../shader_manager.h"
#include "../../util/mesh_factory.h"
#include "../../asset/asset_manager.h"
#include "../../math/math_util.h"

namespace apex {
const bool SkydomeControl::clouds_in_dome = false;

SkydomeControl::SkydomeControl(Camera *camera)
    : EntityControl(50.0), camera(camera), global_time(0.0)
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
    dome->SetLocalScale(50);
    dome->GetChild(0)->GetRenderable()->SetShader(shader);
    dome->GetChild(0)->GetRenderable()->SetRenderBucket(Renderable::RB_SKY);
    dome->GetChild(0)->GetRenderable()->GetMaterial().depth_test = false;
    dome->GetChild(0)->GetRenderable()->GetMaterial().depth_write = false;
    dome->GetChild(0)->GetRenderable()->GetMaterial().alpha_blended = true;

    if (!clouds_in_dome) {
        ShaderProperties clouds_defines;
        clouds_shader = ShaderManager::GetInstance()->GetShader<CloudsShader>(clouds_defines);
        clouds_shader->SetCloudColor(Vector4(1.0));

        clouds_quad = MeshFactory::CreateQuad();
        clouds_quad->SetShader(clouds_shader);
        clouds_quad->SetRenderBucket(Renderable::RB_SKY);
        clouds_quad->GetMaterial().depth_test = false;
        clouds_quad->GetMaterial().depth_write = false;
        clouds_quad->GetMaterial().alpha_blended = true;

        auto clouds_node = std::make_shared<Entity>("clouds");
        clouds_node->Rotate(Quaternion(Vector3::UnitX(), MathUtil::PI / 2.0f));
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
    if (!clouds_in_dome) {
        clouds_shader->SetGlobalTime(global_time);
    } else {
        shader->SetGlobalTime(global_time);
    }
}
} // namespace apex