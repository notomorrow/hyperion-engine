#include "grass_populator.h"

#include "../../asset/asset_manager.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/vegetation_shader.h"

namespace hyperion {
GrassPopulator::GrassPopulator(
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
        fbom::FBOMObjectType("GRASS_POPULATOR_CONTROL"),
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

GrassPopulator::~GrassPopulator()
{
}

std::shared_ptr<Node> GrassPopulator::CreateEntity(const Vector3 &position) const
{
    auto object_node = AssetManager::GetInstance()->LoadFromFile<Node>("models/grass/grass2.obj");
    // auto object_node = AssetManager::GetInstance()->LoadFromFile<Entity>("models/cube.obj");
    // auto object_node = std::make_shared<Entity>("Populator object"); // TODO: virtual method
    // object_node->SetLocalRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(180.0f)));
    // object_node->SetLocalRotation(Quaternion(lookat_mat) * Quaternion(Vector3::UnitY(), MathUtil::Random(0.0f, MathUtil::DegToRad(359.0f))));
    object_node->SetLocalTranslation(position);
    object_node->SetLocalScale(Vector3(2.0f) + MathUtil::Random(-0.5f, 0.5f));
    object_node->SetLocalRotation(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(MathUtil::Random(0, 359))));

    for (int i = 0; i < object_node->NumChildren(); i++) {
        if (auto child = object_node->GetChild(i)) {
            if (auto renderable = object_node->GetChild(i)->GetRenderable()) {
                renderable->SetShader(
                    ShaderManager::GetInstance()->GetShader<LightingShader>(
                        ShaderProperties()
                            .Define("DEFERRED", false)
                            .Define("VEGETATION_FADE", true)
                            .Define("VEGETATION_LIGHTING", false)
                    )
                );
            }
            object_node->GetChild(i)->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
            object_node->GetChild(i)->GetMaterial().alpha_blended = true;
            object_node->GetChild(i)->GetMaterial().cull_faces = MaterialFace_None;
            object_node->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_FLIP_UV, Vector2(0, 1));
        }
    }

    object_node->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
    object_node->Update(1.0f);
    
    return object_node;
}

std::shared_ptr<Control> GrassPopulator::CloneImpl()
{
    return std::make_shared<GrassPopulator>(nullptr, m_seed, m_probability_factor,
        m_tolerance, m_max_distance, m_spread, m_num_entities_per_chunk,
        m_num_patches); // TODO
}
} // namespace hyperion
