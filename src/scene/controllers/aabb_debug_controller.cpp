#include "aabb_debug_controller.h"
#include <engine.h>
#include <builders/mesh_builder.h>

#include <util/utf8.hpp>

namespace hyperion::v2 {

AabbDebugController::AabbDebugController(Engine *engine)
    : Controller("AabbDebugController"),
      m_engine(engine)
{
}

void AabbDebugController::OnAdded()
{
    m_aabb = GetParent()->GetWorldAabb();

    AssertThrow(m_engine != nullptr);

    auto mesh              = MeshBuilder::Cube();
    auto vertex_attributes = mesh->GetVertexAttributes();
    auto material          = m_engine->resources.materials.Add(std::make_unique<Material>("aabb_material"));

    auto shader            = m_engine->shader_manager.GetShader(ShaderManager::Key::DEBUG_AABB).IncRef();
    const auto shader_id   = shader != nullptr ? shader->GetId() : Shader::empty_id;

    m_aabb_entity = m_engine->resources.spatials.Add(
        std::make_unique<Spatial>(
            m_engine->resources.meshes.Add(std::move(mesh)),
            std::move(shader),
            std::move(material),
            RenderableAttributeSet {
                .bucket            = Bucket::BUCKET_TRANSLUCENT,
                .shader_id         = shader_id,
                .vertex_attributes = vertex_attributes,
                .fill_mode         = FillMode::LINE,
                .cull_faces        = FaceCullMode::BACK
            }
        )
    );

    m_aabb_entity.Init();

    // TODO: add to root node
}

void AabbDebugController::OnRemoved()
{
    if (m_aabb_entity != nullptr) {
        // TODO remove from root
        m_aabb_entity = nullptr;
    }
}

void AabbDebugController::OnUpdate(GameCounter::TickUnit delta)
{
    if (m_aabb != GetParent()->GetWorldAabb()) {
        m_aabb = GetParent()->GetWorldAabb();
    }

    if (m_aabb_entity == nullptr) {
        return;
    }

    // m_aabb.SetCenter(Vector3::Zero());

    m_aabb_entity->SetTransform(Transform(
        GetParent()->GetWorldTranslation() + Vector3(0, m_aabb.GetDimensions().y * 0.5f, 0),
        m_aabb.GetDimensions() * 0.5f,
        Quaternion::Identity()
    ));

    m_aabb_entity->Update(m_engine);

    // m_aabb_entity->SetTransform(GetParent()->GetWorldTransform());
}

} // namespace hyperion::v2