#include <ui/UIObject.hpp>
#include <ui/UIScene.hpp>

#include <util/MeshBuilder.hpp>

#include <math/MathUtil.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>

#include <system/Application.hpp>

#include <input/InputManager.hpp>

#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

// UIObject

UIObject::UIObject(ID<Entity> entity, UIScene *parent)
    : m_entity(entity),
      m_parent(parent)
{
    AssertThrowMsg(entity.IsValid(), "Invalid Entity provided to UIObject!");
    AssertThrowMsg(parent != nullptr, "Invalid UIScene parent pointer provided to UIObject!");

    AddToScene();

    struct ScriptedDelegate
    {
        ID<Entity>  entity;
        UIScene     *parent;
        String      method_name;

        ScriptedDelegate(UIObject *ui_object, const String &method_name)
            : entity(ui_object->GetEntity()),
              parent(ui_object->GetParent()),
              method_name(method_name)
        {
        }

        bool operator()(const UIMouseEventData &)
        {
            if (!entity.IsValid() || !parent) {
                return false;
            }

            ScriptComponent *script_component = parent->GetScene()->GetEntityManager()->TryGetComponent<ScriptComponent>(entity);

            if (!script_component || !script_component->object) {
                return false;
            }
            
            if (dotnet::Class *class_ptr = script_component->object->GetClass()) {
                if (auto *method_ptr = class_ptr->GetMethod(method_name)) {
                    // Stubbed method, do not call
                    if (method_ptr->HasAttribute("Hyperion.ScriptMethodStub")) {
                        return false;
                    }

                    return script_component->object->InvokeMethod<bool>(method_ptr);
                }
            }

            return false;
        }
    };

    OnMouseHover.Bind(ScriptedDelegate { this, "OnMouseHover" });
    OnMouseDrag.Bind(ScriptedDelegate { this, "OnMouseDrag" });
    OnMouseUp.Bind(ScriptedDelegate { this, "OnMouseUp" });
    OnMouseDown.Bind(ScriptedDelegate { this, "OnMouseDown" });
    OnClick.Bind(ScriptedDelegate { this, "OnClick" });
}

UIObject::~UIObject()
{
}

void UIObject::AddToScene()
{
    AssertThrow(m_entity.IsValid());
    AssertThrow(m_parent != nullptr);

    const Handle<Scene> &scene = m_parent->GetScene();
    AssertThrow(scene.IsValid());

    Handle<Mesh> mesh = MeshBuilder::Quad();
    InitObject(mesh);

    scene->GetEntityManager()->AddComponent(m_entity, MeshComponent {
        mesh,
        GetMaterial()
    });

    scene->GetEntityManager()->AddComponent(m_entity, VisibilityStateComponent {
        // VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
    });

    scene->GetEntityManager()->AddComponent(m_entity, BoundingBoxComponent {
        mesh->GetAABB()
    });

    scene->GetEntityManager()->AddComponent(m_entity, TransformComponent {
        Transform { }
    });
}

Name UIObject::GetName() const
{
    return m_name;
}

void UIObject::SetName(Name name)
{
    m_name = name;
}

Vec2i UIObject::GetPosition() const
{
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        // Invalid entity or parent
        return Vec2i::Zero();
    }

    if (TransformComponent *transform_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
        const Vec3f &translation = transform_component->transform.GetTranslation();

        return Vec2i {
            MathUtil::Floor<float, int>(translation.x),
            MathUtil::Floor<float, int>(translation.y)
        };
    }

    return Vec2i::Zero();
}

void UIObject::SetPosition(Vec2i position)
{
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        // Invalid entity or parent
        return;
    }

    if (TransformComponent *transform_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
        transform_component->transform.SetTranslation(Vec3f {
            float(position.x),
            float(position.y),
            0.0f
        });
    }
}

Vec2i UIObject::GetSize() const
{
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        // Invalid entity or parent
        return Vec2i::Zero();
    }

    if (TransformComponent *transform_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
        const Vec3f &scale = transform_component->transform.GetScale();

        return Vec2i {
            MathUtil::Floor<float, int>(scale.x),
            MathUtil::Floor<float, int>(scale.y)
        };
    }

    return Vec2i::Zero();
}

void UIObject::SetSize(Vec2i size)
{
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        // Invalid entity or parent
        return;
    }

    if (TransformComponent *transform_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
        transform_component->transform.SetScale(Vec3f {
            float(size.x),
            float(size.y),
            1.0f
        });
    }
}

Handle<Material> UIObject::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_mode         = BlendMode::NORMAL,
            .cull_faces         = FaceCullMode::NONE,
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.0f, 0.005f, 0.015f, 0.95f } }
        }
    );
}

} // namespace hyperion::v2
