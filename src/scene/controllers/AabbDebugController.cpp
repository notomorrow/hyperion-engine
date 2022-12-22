#include "AabbDebugController.hpp"
#include <Engine.hpp>
#include <util/MeshBuilder.hpp>

#include <util/UTF8.hpp>

namespace hyperion::v2 {

AABBDebugController::AABBDebugController()
    : Controller("AABBDebugController", false)
{
}

void AABBDebugController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        scene->AddEntity(m_aabb_entity);
    }
}

void AABBDebugController::OnDetachedFromScene(ID<Scene> id)
{
    if (!m_aabb_entity) {
        return;
    }

    if (auto scene = Handle<Scene>(id)) {
        scene->RemoveEntity(m_aabb_entity);
    }
}

void AABBDebugController::OnAdded()
{
    m_aabb = GetOwner()->GetWorldAABB();

    auto mesh = MeshBuilder::Cube();
    auto vertex_attributes = mesh->GetVertexAttributes();

    auto material = CreateObject<Material>("aabb_material");

    auto shader = Engine::Get()->shader_manager.GetShader(ShaderManager::Key::DEBUG_AABB);

    m_aabb_entity = CreateObject<Entity>(
        std::move(mesh),
        std::move(shader),
        std::move(material),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .fill_mode = FillMode::LINE,
                .blend_mode = BlendMode::ADDITIVE,
                .cull_faces = FaceCullMode::NONE,
                .flags = 0x0
            }
        ),
        Entity::InitInfo {
            .flags = 0x0 // no flags
        }
    );
}

void AABBDebugController::OnRemoved()
{
    if (m_aabb_entity) {
        const auto scenes = m_aabb_entity->GetScenes();

        for (const auto &id : scenes) {
            m_aabb_entity->SetIsInScene(id, false);
        }

        m_aabb_entity.Reset();
    }
}

void AABBDebugController::OnTransformUpdate(const Transform &transform)
{
    m_aabb = GetOwner()->GetWorldAABB();

    if (m_aabb_entity == nullptr) {
        DebugLog(
            LogType::Warn,
            "No AABB entity set!\n"
        );

        return;
    }

    m_aabb_entity->SetTransform(Transform(
        m_aabb.GetCenter(),
        m_aabb.GetExtent() * 0.5f,
        Quaternion::Identity()
    ));
}

} // namespace hyperion::v2