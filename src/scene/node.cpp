#include "node.h"
#include "util.h"
#include "scene/scene_manager.h"

#include <algorithm>

namespace hyperion {

int Node::NodeId()
{
    static int id = 0;

    return ++id;
}

Node::Node(const std::string &name)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("ENTITY")),
      m_name(name),
      m_aabb_affects_parent(true),
      m_flags(UPDATE_TRANSFORM | UPDATE_AABB),
      m_parent(nullptr),
      m_local_translation(Vector3::Zero()),
      m_local_scale(Vector3::One()),
      m_local_rotation(Quaternion::Identity()),
      m_octree_node_id(NodeId())
{
}

Node::~Node()
{
    SceneManager::GetInstance()->GetOctree()->RemoveNode(m_octree_node_id);

    for (auto it = m_controls.rbegin(); it != m_controls.rend(); ++it) {
        (*it)->OnRemoved();
        (*it)->m_first_run = true;
        (*it)->parent = nullptr;
    }

    m_controls.clear();
    m_children.clear();
}

void Node::SetGlobalTranslation(const Vector3 &translation)
{
    if (m_parent == nullptr) {
        SetLocalTranslation(translation);

        return;
    }

    m_local_translation = translation - m_parent->GetGlobalTransform().GetTranslation();

    SetTransformUpdateFlag();
    SetAABBUpdateFlag();
}

void Node::SetGlobalRotation(const Quaternion &rotation)
{
    if (m_parent == nullptr) {
        SetLocalRotation(rotation);

        return;
    }

    Quaternion tmp = m_parent->GetGlobalTransform().GetRotation();
    tmp.Invert();

    m_local_rotation = rotation * tmp;

    SetTransformUpdateFlag();
    SetAABBUpdateFlag();
}

void Node::SetGlobalScale(const Vector3 &scale)
{
    if (m_parent == nullptr) {
        SetLocalScale(scale);

        return;
    }

    m_local_scale = scale / m_parent->GetGlobalTransform().GetScale();

    SetTransformUpdateFlag();
    SetAABBUpdateFlag();
}

void Node::UpdateTransform()
{
    if (m_parent != nullptr) {
        m_spatial.m_transform.SetTranslation(m_local_translation + m_parent->GetGlobalTransform().GetTranslation());
        m_spatial.m_transform.SetScale(m_local_scale * m_parent->GetGlobalTransform().GetScale());
        m_spatial.m_transform.SetRotation(m_local_rotation * m_parent->GetGlobalTransform().GetRotation());
    } else {
        m_spatial.m_transform = Transform(m_local_translation, m_local_scale, m_local_rotation);
    }
}

void Node::UpdateAABB()
{
    // clear current aabb
    m_spatial.m_aabb.Clear();
    // retrieve renderable's aabb
    if (m_spatial.m_renderable != nullptr) {
        BoundingBox renderable_aabb = m_spatial.m_renderable->GetAABB();

        if (!renderable_aabb.Empty()) {
            BoundingBox renderable_aabb_transformed;
            // multiply by transform
            std::array<Vector3, 8> corners = renderable_aabb.GetCorners();

            for (Vector3 &corner : corners) {
                corner *= m_spatial.m_transform.GetMatrix();

                renderable_aabb_transformed.Extend(corner);
            }

            m_spatial.m_aabb.Extend(renderable_aabb_transformed);
        }
    }

    if (m_aabb_affects_parent && m_parent != nullptr) {
        // multiply parent's bounding box by this one
        m_parent->m_spatial.m_aabb.Extend(m_spatial.m_aabb);
    }
}

float Node::CalculateCameraDistance(Camera *camera) const
{
    return GetGlobalTranslation().Distance(camera->GetTranslation());
}

void Node::AddChild(std::shared_ptr<Node> entity)
{
    ex_assert(entity != nullptr);
    ex_assert(entity->m_parent == nullptr);
    //ex_assert_msg(std::find(m_children.begin(), m_children.end(), entity) == m_children.end(), "object already exists in node");

    if (entity->m_flags & PENDING_REMOVAL) {
        std::cout << entity->GetName() << " saved from death\n";
        entity->m_flags &= ~PENDING_REMOVAL;
        auto it = std::find(
            m_children_pending_removal.begin(),
            m_children_pending_removal.end(),
            entity
        );

        ex_assert(it != m_children_pending_removal.end());

        m_children_pending_removal.erase(it);
        entity->m_flags &= ~PENDING_REMOVAL;
    }

    // find free slot
    bool slot_found = false;

    for (size_t i = 0; i < m_children.size(); i++) {
        if (m_children[i] == nullptr) {
            m_children[i] = entity;
            slot_found = true;

            break;
        }
    }

    if (!slot_found) {
        m_children.push_back(entity);
    }

    entity->m_parent = non_owning_ptr(this);

    entity->SetTransformUpdateFlag();
    //entity->UpdateTransform();

    if (entity->GetAABBAffectsParent()) {
        SetAABBUpdateFlag();
        //entity->UpdateAABB();

        SceneManager::GetInstance()->GetOctree()->InsertNode(entity->m_octree_node_id, entity->m_spatial);
    }
}

void Node::RemoveChild(std::shared_ptr<Node> entity)
{
    ex_assert(entity != nullptr);
    ex_assert(entity->m_parent == this);

    m_children_pending_removal.push_back(entity);

    // fill slot with nullptr, rather than resizing, potentially causing a chain
    // of events that will cause a loop to overflow
    std::replace(m_children.begin(), m_children.end(), entity, std::shared_ptr<Node>(nullptr));

    entity->m_parent = non_owning_ptr<Node>(nullptr);
    entity->SetPendingRemovalFlag();
    entity->SetTransformUpdateFlag();

    if (entity->GetAABBAffectsParent()) {
        SetAABBUpdateFlag();

        SceneManager::GetInstance()->GetOctree()->RemoveNode(entity->m_octree_node_id);
    }
}

std::shared_ptr<Node> Node::GetChild(size_t index) const
{
    return m_children.at(index);
}

std::shared_ptr<Node> Node::GetChild(const std::string &name) const
{
    auto it = std::find_if(m_children.begin(), m_children.end(), [&](const std::shared_ptr<Node> &elt)
    {
        if (elt == nullptr) {
            return false;
        }

        return elt->GetName() == name;
    });

    return it != m_children.end() ? *it : nullptr;
}

std::shared_ptr<Node> Node::GetChildPendingRemoval(size_t index) const
{
    return m_children_pending_removal.at(index);
}

void Node::ClearPendingRemoval()
{
    for (auto &child : m_children_pending_removal) {
        if (child == nullptr) {
            continue;
        }

        child->m_flags &= ~PENDING_REMOVAL;
    }

    m_children_pending_removal.clear();

    size_t num_children = m_children.size();

    for (size_t i = 0; i < num_children; i++) {
        auto &child = m_children[i];

        if (child == nullptr) {
            continue;
        }

        child->ClearPendingRemoval();
    }
}

void Node::AddControl(std::shared_ptr<EntityControl> control)
{
    // ex_assert(control->parent == nullptr);

    m_controls.push_back(control);
    control->parent = this;
    control->OnAdded();
}

void Node::RemoveControl(const std::shared_ptr<EntityControl> &control)
{
    soft_assert(control != nullptr);

    m_controls.erase(std::find(m_controls.begin(), m_controls.end(), control));
    control->OnRemoved();
    control->parent = nullptr;
}

void Node::Update(double dt)
{
    BoundingBox aabb_before(m_spatial.m_aabb);

    if (m_flags & UPDATE_TRANSFORM) {
        UpdateTransform();
        m_flags &= ~UPDATE_TRANSFORM;
    }

    if (m_flags & UPDATE_AABB) {
        UpdateAABB();
        m_flags &= ~UPDATE_AABB;
    }

    UpdateControls(dt);

    size_t num_children = m_children.size();

    for (size_t i = 0; i < num_children; i++) {
        auto &child = m_children[i];

        if (child == nullptr) {
            continue;
        }

        child->Update(dt);
    }

    if (GetAABBAffectsParent() && aabb_before != m_spatial.m_aabb) {
        SceneManager::GetInstance()->GetOctree()->UpdateNode(m_octree_node_id, m_spatial);
    }
}

void Node::UpdateControls(double dt)
{
    for (auto &control : m_controls) {
        if (control->m_first_run) {
            control->OnFirstRun(dt);
            control->m_first_run = false;
        }

        control->tick += dt * 1000;
        if ((control->tick / 1000 * control->tps) >= 1) {
            control->OnUpdate(dt);
            control->tick = 0;
        }
    }
}

void Node::SetTransformUpdateFlag()
{
    m_flags |= UPDATE_TRANSFORM;

    for (auto &child : m_children) {
        if (child == nullptr) {
            continue;
        }

        child->SetTransformUpdateFlag();
    }
}

void Node::SetAABBUpdateFlag()
{
    m_flags |= UPDATE_AABB;

    for (auto &child : m_children) {
        if (child == nullptr) {
            continue;
        }

        child->SetAABBUpdateFlag();
    }
}

void Node::SetPendingRemovalFlag()
{
    m_flags |= PENDING_REMOVAL;

    for (auto &child : m_children) {
        if (child == nullptr) {
            continue;
        }

        child->SetPendingRemovalFlag();
    }
}

std::shared_ptr<Loadable> Node::Clone()
{
    return CloneImpl();
}

std::shared_ptr<Node> Node::CloneImpl()
{
    auto new_entity = std::make_shared<Node>(m_name + "_clone");

    //new_entity->m_flags = m_flags;
    new_entity->m_spatial = m_spatial;

    new_entity->m_local_translation = m_local_translation;
    new_entity->m_local_scale = m_local_scale;
    new_entity->m_local_rotation = m_local_rotation;

    // clone all child entities
    for (auto &child : m_children) {
        if (child == nullptr) {
            continue;
        }

        new_entity->AddChild(child->CloneImpl());
    }

    return new_entity;
}

} // namespace hyperion
