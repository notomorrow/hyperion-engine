#include "octree.h"
#include "spatial.h"
#include "scene/octree.h"

namespace hyperion::v2 {

Octree::Octree(const BoundingBox &aabb)
    : m_aabb(aabb),
      m_parent(nullptr),
      m_is_divided(false),
      m_root(nullptr),
      m_visibility_state{}
{
    InitOctants();
}

Octree::~Octree()
{
    AssertThrowMsg(m_nodes.empty(), "Expected nodes to be emptied before octree destructor");
}

void Octree::SetParent(Octree *parent)
{
    m_parent = parent;

    if (m_parent != nullptr) {
        m_root = m_parent->m_root;
    } else {
        m_root = nullptr;
    }
}

bool Octree::EmptyDeep(int depth) const
{
    if (!Empty()) {
        return false;
    }

    if (!m_is_divided) {
        return true;
    }

    if (depth != 0) {
        return std::all_of(m_octants.begin(), m_octants.end(), [depth](const Octant &octant) {
            return octant.octree->EmptyDeep(depth - 1);
        });
    }

    return true;
}

void Octree::InitOctants()
{
    const Vector3 divided_aabb_dimensions = m_aabb.GetDimensions() / 2;

    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            for (int z = 0; z < 2; z++) {
                int index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + divided_aabb_dimensions * Vector3(float(x), float(y), float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vector3(float(x), float(y), float(z)) + Vector3(1.0f))
                    )
                };
            }
        }
    }
}

void Octree::Divide(Engine *engine)
{
    AssertThrow(!m_is_divided);

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree == nullptr);

        octant.octree = std::make_unique<Octree>(octant.aabb);
        octant.octree->SetParent(this);

        if (m_root != nullptr) {
            m_root->events.on_insert_octant(engine, octant.octree.get(), nullptr);
        }
    }

    m_is_divided = true;
}

void Octree::Undivide(Engine *engine)
{
    AssertThrow(m_is_divided);
    AssertThrowMsg(m_nodes.empty(), "Undivide() should be called on octrees with no remaining nodes");

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->m_is_divided) {
            octant.octree->Undivide(engine);
        }
        
        if (m_root != nullptr) {
            m_root->events.on_remove_octant(engine, octant.octree.get(), nullptr);
        }

        octant.octree.reset();
    }

    m_is_divided = false;
}

void Octree::CollapseParents(Engine *engine)
{
    if (m_is_divided || !Empty()) {
        return;
    }

    Octree *iteration = m_parent,
           *highest_empty = nullptr;

    while (iteration != nullptr && iteration->Empty()) {
        for (const Octant &octant : iteration->m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree.get() == highest_empty) {
                /* Do not perform a check on our node, as we've already
                 * checked it by going up the chain.
                 * As `iter` becomes the parent of this node we're currently working with,
                 * we will not have to perform duplicate EmptyDeep() checks on any octants because of this check.
                 */
                continue;
            }

            if (!octant.octree->EmptyDeep()) {
                goto undivide;
            }
        }

        highest_empty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highest_empty != nullptr) {
        highest_empty->Undivide(engine);
    }
}

void Octree::Clear(Engine *engine)
{
    for (auto &node : m_nodes) {
        node.spatial->OnRemovedFromOctree(this);

        if (m_root != nullptr) {
            auto it = m_root->node_to_octree.find(node.spatial);

            if (it != m_root->node_to_octree.end()) {
                m_root->node_to_octree.erase(it);
            }
        }
    }

    m_nodes.clear();

    if (!m_is_divided) {
        return;
    }

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        octant.octree->Clear(engine);
    }

    Undivide(engine);
}

bool Octree::Insert(Engine *engine, Spatial *spatial)
{
    for (Octant &octant : m_octants) {
        if (octant.aabb.Contains(spatial->GetWorldAabb())) {
            if (!m_is_divided) {
                Divide(engine);
            }

            AssertThrow(octant.octree != nullptr);

            return octant.octree->Insert(engine, spatial);
        }
    }

    return InsertInternal(engine, spatial);
}

bool Octree::InsertInternal(Engine *engine, Spatial *spatial)
{
    m_nodes.push_back(Node{
        .spatial = spatial,
        .aabb = spatial->GetWorldAabb(),
        .visibility_state = &m_visibility_state
    });

    spatial->OnAddedToOctree(this);

    if (m_root != nullptr) {
        AssertThrowMsg(m_root->node_to_octree[spatial] == nullptr, "Spatial must not already be in octree hierarchy.");

        m_root->node_to_octree[spatial] = this;
        m_root->events.on_insert_node(engine, this, spatial);
    }

    return true;
}

bool Octree::Remove(Engine *engine, Spatial *spatial)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(spatial);

        if (it == m_root->node_to_octree.end()) {
            return false;
        }

        if (auto *octree = it->second) {
            m_root->node_to_octree.erase(it);

            return octree->RemoveInternal(engine, spatial);
        }

        return false;
    }

    /* TODO: Dec ref count */

    if (!m_aabb.Contains(spatial->GetWorldAabb())) {
        return false;
    }

    return RemoveInternal(engine, spatial);
}

bool Octree::RemoveInternal(Engine *engine, Spatial *spatial)
{
    const auto it = FindNode(spatial);

    if (it == m_nodes.end()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->RemoveInternal(engine, spatial)) {
                    return true;
                }
            }
        }

        return false;
    }

    if (m_root != nullptr) {
        m_root->events.on_remove_node(engine, this, spatial);
    }

    m_nodes.erase(it);

    if (!m_is_divided && m_nodes.empty()) {
        if (Octree *parent = m_parent) {
            int depth = DEPTH_SEARCH_INF; /* first time, bite the bullet and search all nested octants. after that we only search the next layer */

            while (parent->EmptyDeep(depth)) {
                if (parent->m_parent == nullptr) {
                    /* At root, call Undivide() to collapse nodes */
                    parent->Undivide(engine);

                    break;
                }

                parent = parent->m_parent;
                depth = 1; /* only search next layer */
            }
        }
    }

    spatial->OnRemovedFromOctree(this);

    return true;
}

bool Octree::Update(Engine *engine, Spatial *spatial)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(spatial);

        if (it == m_root->node_to_octree.end()) {
            return false;
        }

        if (auto *octree = it->second) {
            return octree->UpdateInternal(engine, spatial);
        }

        return false;
    }

    return UpdateInternal(engine, spatial);
}

bool Octree::UpdateInternal(Engine *engine, Spatial *spatial)
{
    const auto it = FindNode(spatial);

    if (it == m_nodes.end()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->UpdateInternal(engine, spatial)) {
                    return true;
                }
            }
        }

        return false;
    }

    const auto &new_aabb = spatial->GetWorldAabb();
    const auto &old_aabb = it->aabb;

    if (new_aabb == old_aabb) {
        /* Aabb has not changed - no need to update */
        return true;
    }

    /* Aabb has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    spatial->OnRemovedFromOctree(this);

    if (m_root != nullptr) {
        m_root->events.on_remove_node(engine, this, spatial);
        m_root->node_to_octree[spatial] = nullptr;
    }

    m_nodes.erase(it);

    if (IsRoot() || m_aabb.Contains(new_aabb)) {
        return Insert(engine, spatial);
    }

    bool inserted = false;
    
    /* Contains is false at this point */
    Octree *parent = m_parent;

    while (parent != nullptr) {
        if (parent->m_aabb.Contains(new_aabb)) {
            inserted = parent->Insert(engine, spatial);

            break;
        }

        parent = parent->m_parent;
    }

    /* Node has now been added to it's appropriate octant -- remove any potential empty octants */
    CollapseParents(engine);

    return inserted;
}

void Octree::CalculateVisibility(Scene *scene)
{
    if (scene == nullptr) {
        return;
    }

    if (scene->GetId().value - 1 >= max_scenes) {
        DebugLog(
            LogType::Error,
            "Scene #%lu out of bounds of octree scene visibility array. Cannot update visibility state.\n",
            scene->GetId().value
        );

        return;
    }

    const auto &frustum = scene->GetCamera()->GetFrustum();
     
    if (frustum.BoundingBoxInFrustum(m_aabb)) {
        UpdateVisibilityState(scene);
    }
}

void Octree::UpdateVisibilityState(Scene *scene)
{
    /* assume we are already visible from CalculateVisibility() check */
    const auto &frustum = scene->GetCamera()->GetFrustum();

    m_visibility_state.Set(scene->GetId(), true);

    if (!m_is_divided) {
        return;
    }

    for (auto &octant : m_octants) {
        if (!frustum.BoundingBoxInFrustum(octant.aabb)) {
            continue;
        }

        octant.octree->UpdateVisibilityState(scene);
    }
}

void Octree::OnSpatialRemoved(Engine *engine, Spatial *spatial)
{
    if (spatial == nullptr) {
        return;
    }
    
    if (!Remove(engine, spatial)) {
        DebugLog(LogType::Error, "Failed to find Spatial #%lu in octree\n", spatial->GetId().value);
    }
}



} // namespace hyperion::v2