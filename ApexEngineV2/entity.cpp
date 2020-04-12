#include "entity.h"

#include <algorithm>

namespace apex {

Entity::Entity(const std::string &name)
    : m_name(name),
      m_flags(0),
      m_parent(nullptr),
      m_local_translation(Vector3::Zero()),
      m_local_scale(Vector3::One()),
      m_local_rotation(Quaternion::Identity())
{
}

Entity::~Entity()
{
    for (auto it = m_controls.rbegin(); it != m_controls.rend(); ++it) {
        (*it)->OnRemoved();
        (*it)->parent = nullptr;
    }
    m_controls.clear();
    m_children.clear();
}

void Entity::UpdateTransform()
{
    if (m_parent != nullptr) {
        m_global_transform.SetTranslation(m_local_translation + m_parent->GetGlobalTransform().GetTranslation());
        m_global_transform.SetScale(m_local_scale * m_parent->GetGlobalTransform().GetScale());
        m_global_transform.SetRotation(m_local_rotation * m_parent->GetGlobalTransform().GetRotation());
    } else {
        m_global_transform = Transform(m_local_translation, m_local_scale, m_local_rotation);
    }
}

void Entity::UpdateAABB()
{
    // clear current aabb
    m_aabb.Clear();
    // retrieve renderable's aabb
    if (m_renderable != nullptr) {
        BoundingBox renderable_aabb = m_renderable->GetAABB();

        if (!renderable_aabb.Empty()) {
            BoundingBox renderable_aabb_transformed;
            // multiply by transform
            std::array<Vector3, 8> corners = renderable_aabb.GetCorners();
            for (Vector3 &corner : corners) {
                corner *= m_global_transform.GetMatrix();
                renderable_aabb_transformed.Extend(corner);
            }
            m_aabb.Extend(renderable_aabb_transformed);
        }
    }

    if (m_parent != nullptr) {
        // multiply parent's bounding box by this one
        m_parent->m_aabb.Extend(m_aabb);
    }
}

void Entity::AddChild(std::shared_ptr<Entity> entity)
{
    m_children.push_back(entity);
    entity->m_parent = this;
    entity->SetTransformUpdateFlag();
}

void Entity::RemoveChild(const std::shared_ptr<Entity> &entity)
{
    m_children.erase(std::find(m_children.begin(), m_children.end(), entity));
    entity->m_parent = nullptr;
    entity->SetTransformUpdateFlag();
}

std::shared_ptr<Entity> Entity::GetChild(size_t index) const
{
    return m_children.at(index);
}

std::shared_ptr<Entity> Entity::GetChild(const std::string &name) const
{
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [&](const std::shared_ptr<Entity> &elt)
    {
        return elt->GetName() == name;
    });

    return it != m_children.end() ? *it : nullptr;
}

void Entity::AddControl(std::shared_ptr<EntityControl> control)
{
    m_controls.push_back(control);
    control->parent = this;
    control->OnAdded();
}

void Entity::RemoveControl(const std::shared_ptr<EntityControl> &control)
{
    m_controls.erase(std::find(m_controls.begin(), m_controls.end(), control));
    control->OnRemoved();
    control->parent = nullptr;
}

void Entity::Update(double dt)
{
    if (m_flags & UPDATE_TRANSFORM) {
        UpdateTransform();
        m_flags &= ~UPDATE_TRANSFORM;
    }
    if (m_flags & UPDATE_AABB) {
        UpdateAABB();
        m_flags &= ~UPDATE_AABB;
    }

    for (auto &control : m_controls) {
        control->tick += dt * 1000;
        if ((control->tick / 1000 * control->tps) >= 1) {
            control->OnUpdate(dt);
            control->tick = 0;
        }
    }

    for (auto &child : m_children) {
        child->Update(dt);
    }
}

void Entity::SetTransformUpdateFlag()
{
    m_flags |= UPDATE_TRANSFORM;
    for (auto &child : m_children) {
        child->SetTransformUpdateFlag();
    }
}

void Entity::SetAABBUpdateFlag()
{
    m_flags |= UPDATE_AABB;
    for (auto &child : m_children) {
        child->SetAABBUpdateFlag();
    }
}

std::shared_ptr<Loadable> Entity::Clone()
{
    return CloneImpl();
}

std::shared_ptr<Entity> Entity::CloneImpl()
{
    auto new_entity = std::make_shared<Entity>(m_name + "_clone");

    new_entity->m_flags = m_flags;

    new_entity->m_material = m_material;

    // reference copy
    new_entity->m_renderable = m_renderable;

    new_entity->m_local_translation = m_local_translation;
    new_entity->m_local_scale = m_local_scale;
    new_entity->m_local_rotation = m_local_rotation;

    // clone all child entities
    for (auto &child : m_children) {
        new_entity->AddChild(child->CloneImpl());
    }

    return new_entity;
}

} // namespace apex
