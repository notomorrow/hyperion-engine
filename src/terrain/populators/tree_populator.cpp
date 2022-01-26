#include "tree_populator.h"

#include "../../asset/asset_manager.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/vegetation_shader.h"

namespace hyperion {
TreePopulator::TreePopulator(
    Camera *camera,
    unsigned long seed,
    double probability_factor,
    float tolerance,
    float max_distance,
    float spread,
    int num_entities_per_chunk,
    int num_patches
)
    : Populator(
        camera,
        seed,
        probability_factor,
        tolerance,
        max_distance,
        spread,
        num_entities_per_chunk,
        num_patches
    )
{
}

TreePopulator::~TreePopulator()
{
}

std::shared_ptr<Entity> TreePopulator::CreateEntity(const Vector3 &position) const
{
    auto tree = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/pine/LoblollyPine.obj", true);

    for (int i = 0; i < tree->NumChildren(); i++) {
        tree->GetChild(i)->GetMaterial().SetParameter("shininess", 0.0f);
        tree->GetChild(i)->GetMaterial().SetParameter("roughness", 0.9f);
    }

    tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
    // tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().alpha_blended = true;
    tree->GetChild("LoblollyPineNeedles_1")->GetRenderable()->SetShader(
        ShaderManager::GetInstance()->GetShader<VegetationShader>(
            ShaderProperties()
                .Define("VEGETATION_FADE", false)
                .Define("VEGETATION_LIGHTING", true)
        )
    );
    tree->GetChild("LoblollyPineNeedles_1")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
    tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().SetParameter("RimShading", 0.0f);
    tree->SetLocalTranslation(position);
    tree->SetLocalScale(Vector3(1.3f) + MathUtil::Random(-0.5f, 0.5f));
    tree->SetLocalRotation(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(MathUtil::Random(0, 359))));

    return tree;
}
} // namespace hyperion
