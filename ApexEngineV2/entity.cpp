#include "entity.h"

namespace apex {
Entity::Entity(const std::string &name)
    : name(name),
    flags(0),
    parent(nullptr),
    local_translation(Vector3::Zero()),
    local_scale(Vector3::One()),
    local_rotation(Quaternion::Identity())
{
}

Entity::~Entity()
{
    for (auto it = controls.rbegin(); it != controls.rend(); ++it) {
        (*it)->OnRemoved();
        (*it)->parent = nullptr;
    }
    controls.clear();
    children.clear();
}

const std::string &Entity::GetName() const
{
    return name;
}

void Entity::SetName(const std::string &str)
{
    name = str;
}

const Vector3 &Entity::GetLocalTranslation() const
{
    return local_translation;
}

void Entity::SetLocalTranslation(const Vector3 &vec)
{
    local_translation = vec;
    SetTransformUpdateFlag();
}

const Vector3 &Entity::GetLocalScale() const
{
    return local_scale;
}

void Entity::SetLocalScale(const Vector3 &vec)
{
    local_scale = vec;
    SetTransformUpdateFlag();
}

const Quaternion &Entity::GetLocalRotation() const
{
    return local_rotation;
}

void Entity::SetLocalRotation(const Quaternion &rot)
{
    local_rotation = rot;
    SetTransformUpdateFlag();
}

const Transform &Entity::GetGlobalTransform() const
{
    return global_transform;
}

void Entity::Move(const Vector3 &vec)
{
    SetLocalTranslation(local_translation + vec);
}

void Entity::Scale(const Vector3 &vec)
{
    SetLocalScale(local_scale * vec);
}

void Entity::Rotate(const Quaternion &rot)
{
    SetLocalRotation(local_rotation * rot);
}

void Entity::UpdateTransform()
{
    if (parent != nullptr) {
        global_transform.SetTranslation(local_translation + parent->GetGlobalTransform().GetTranslation());
        global_transform.SetScale(local_scale * parent->GetGlobalTransform().GetScale());
        global_transform.SetRotation(local_rotation * parent->GetGlobalTransform().GetRotation());
    } else {
        global_transform = Transform(local_translation, local_scale, local_rotation);
    }
}

Entity *Entity::GetParent() const
{
    return parent;
}

void Entity::AddChild(std::shared_ptr<Entity> entity)
{
    children.push_back(entity);
    entity->parent = this;
    entity->SetTransformUpdateFlag();
}

void Entity::RemoveChild(const std::shared_ptr<Entity> &entity)
{
    children.erase(std::find(children.begin(), children.end(), entity));
    entity->parent = nullptr;
    entity->SetTransformUpdateFlag();
}

std::shared_ptr<Entity> Entity::GetChild(size_t index) const
{
    return children.at(index);
}

std::shared_ptr<Entity> Entity::GetChild(const std::string &name) const
{
    auto it = std::find_if(children.begin(), children.end(),
        [&](const std::shared_ptr<Entity> &elt)
    {
        return elt->GetName() == name;
    });

    if (it != children.end()) {
        return (*it);
    } else {
        return nullptr;
    }
}

size_t Entity::NumChildren() const
{
    return children.size();
}

void Entity::AddControl(std::shared_ptr<EntityControl> control)
{
    controls.push_back(control);
    control->parent = this;
    control->OnAdded();
}

void Entity::RemoveControl(const std::shared_ptr<EntityControl> &control)
{
    controls.erase(std::find(controls.begin(), controls.end(), control));
    control->OnRemoved();
    control->parent = nullptr;
}

std::shared_ptr<EntityControl> Entity::GetControl(size_t index) const
{
    return controls.at(index);
}

size_t Entity::NumControls() const
{
    return controls.size();
}

std::shared_ptr<Renderable> Entity::GetRenderable() const
{
    return renderable;
}

void Entity::SetRenderable(const std::shared_ptr<Renderable> &ren)
{
    renderable = ren;
}

void Entity::Update(double dt)
{
    for (auto &&control : controls) {
        control->tick += dt * 1000;
        if ((control->tick / 1000 * control->tps) >= 1) {
            control->OnUpdate(dt);
            control->tick = 0;
        }
    }

    if (flags & UPDATE_TRANSFORM) {
        UpdateTransform();
        flags &= ~UPDATE_TRANSFORM;
    }

    for (auto &&child : children) {
        child->Update(dt);
    }
}

void Entity::SetTransformUpdateFlag()
{
    flags |= UPDATE_TRANSFORM;
    for (auto &&child : children) {
        child->SetTransformUpdateFlag();
    }
}
}