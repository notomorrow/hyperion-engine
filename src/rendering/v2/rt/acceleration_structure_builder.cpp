#include "acceleration_structure_builder.h"
#include "../engine.h"

namespace hyperion::v2 {

AccelerationStructureBuilder::AccelerationStructureBuilder(const Spatial *spatial)
    : m_spatial(spatial)
{
}

std::unique_ptr<BottomLevelAccelerationStructure> AccelerationStructureBuilder::Build(Engine *engine)
{
    if (m_spatial == nullptr) {
        return nullptr;
    }

    CreateGeometry(engine);

    auto acceleration_structure = std::make_unique<BottomLevelAccelerationStructure>();
    acceleration_structure->SetTransform(m_spatial->GetTransform().GetMatrix());
    acceleration_structure->AddGeometry(std::move(m_geometry));

    HYPERION_ASSERT_RESULT(acceleration_structure->Create(engine->GetInstance()));

    return std::move(acceleration_structure);
}

void AccelerationStructureBuilder::CreateGeometry(Engine *engine)
{
    const auto *mesh = m_spatial->GetMesh();

    if (m_geometry != nullptr) {
        DestroyGeometry(engine);
    }

    if (mesh == nullptr) {
        return;
    }

    m_geometry = std::make_unique<AccelerationGeometry>(
        mesh->BuildPackedVertices(),
        mesh->BuildPackedIndices()
    );
}

void AccelerationStructureBuilder::DestroyGeometry(Engine *engine)
{
    auto result = renderer::Result::OK;

    if (m_geometry == nullptr) {
        return;
    }
    
    HYPERION_PASS_ERRORS(
        m_geometry->Destroy(engine->GetInstance()),
        result
    );

    m_geometry.reset();

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2