#include "entity.h"

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

const std::string &Entity::GetName() const
{
    return m_name;
}

void Entity::SetName(const std::string &name)
{
    m_name = name;
}

const Vector3 &Entity::GetLocalTranslation() const
{
    return m_local_translation;
}

void Entity::SetLocalTranslation(const Vector3 &translation)
{
    m_local_translation = translation;
    SetTransformUpdateFlag();
}

const Vector3 &Entity::GetLocalScale() const
{
    return m_local_scale;
}

void Entity::SetLocalScale(const Vector3 &scale)
{
    m_local_scale = scale;
    SetTransformUpdateFlag();
}

const Quaternion &Entity::GetLocalRotation() const
{
    return m_local_rotation;
}

void Entity::SetLocalRotation(const Quaternion &rotation)
{
    m_local_rotation = rotation;
    SetTransformUpdateFlag();
}

const Transform &Entity::GetGlobalTransform() const
{
    return m_global_transform;
}

void Entity::Move(const Vector3 &vec)
{
    SetLocalTranslation(m_local_translation + vec);
}

void Entity::Scale(const Vector3 &vec)
{
    SetLocalScale(m_local_scale * vec);
}

void Entity::Rotate(const Quaternion &rot)
{
    SetLocalRotation(m_local_rotation * rot);
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

Entity *Entity::GetParent() const
{
    return m_parent;
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

    if (it != m_children.end()) {
        return (*it);
    } else {
        return nullptr;
    }
}

size_t Entity::NumChildren() const
{
    return m_children.size();
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

std::shared_ptr<EntityControl> Entity::GetControl(size_t index) const
{
    return m_controls.at(index);
}

size_t Entity::NumControls() const
{
    return m_controls.size();
}

std::shared_ptr<Renderable> Entity::GetRenderable() const
{
    return m_renderable;
}

void Entity::SetRenderable(const std::shared_ptr<Renderable> &renderable)
{
    m_renderable = renderable;
}

void Entity::Update(double dt)
{
    if (m_flags & UPDATE_TRANSFORM) {
        UpdateTransform();
        m_flags &= ~UPDATE_TRANSFORM;
    }

    for (auto &&control : m_controls) {
        control->tick += dt * 1000;
        if ((control->tick / 1000 * control->tps) >= 1) {
            control->OnUpdate(dt);
            control->tick = 0;
        }
    }

    for (auto &&child : m_children) {
        child->Update(dt);
    }
}

void Entity::SetTransformUpdateFlag()
{
    m_flags |= UPDATE_TRANSFORM;
    for (auto &&child : m_children) {
        child->SetTransformUpdateFlag();
    }
}
} // namespace apex