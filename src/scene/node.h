#ifndef NODE_H
#define NODE_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

#include "../controls/entity_control.h"
#include "../hash_code.h"
#include "../asset/loadable.h"

#include "spatial.h"
#include "../math/transform.h"
#include "../rendering/renderable.h"
#include "../rendering/material.h"
#include "../util/non_owning_ptr.h"

#include "../asset/fbom/fbom.h"

namespace hyperion {
class Camera;
class Node;
class Octree;

using NodeCallback_t = std::function<void(const std::shared_ptr<Node> &)>;
using PendingNode_t = std::pair<std::shared_ptr<Node>, NodeCallback_t>;

class Node : public fbom::FBOMLoadable {
    static std::mutex add_pending_mutex;
public:
    enum UpdateFlags {
        UPDATE_TRANSFORM = 0x01,
        UPDATE_AABB = 0x02,
        PENDING_REMOVAL = 0x04,
        PENDING_ADDITION = 0x08,
        NODES_ENQUEUED = 0x10
    };

    Node(const std::string &name = "<Unnamed>");
    Node(const Node &other) = delete;
    Node &operator=(const Node &other) = delete;
    virtual ~Node();

    inline int GetFlags() const { return m_flags; }

    inline const std::string &GetName() const { return m_name; }
    inline void SetName(const std::string &name) { m_name = name; }

    inline bool GetAABBAffectsParent() const { return m_aabb_affects_parent; }
    inline void SetAABBAffectsParent(bool value) { m_aabb_affects_parent = value; }

    inline const Vector3 &GetLocalTranslation() const { return m_local_translation; }
    inline void SetLocalTranslation(const Vector3 &translation) 
    { 
        m_local_translation = translation;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Vector3 &GetGlobalTranslation() const { return m_spatial.m_transform.GetTranslation(); }
    void SetGlobalTranslation(const Vector3 &translation);

    inline const Quaternion &GetLocalRotation() const { return m_local_rotation; }
    inline void SetLocalRotation(const Quaternion &rotation) 
    { 
        m_local_rotation = rotation;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Quaternion &GetGlobalRotation() const { return m_spatial.m_transform.GetRotation(); }
    void SetGlobalRotation(const Quaternion &rotation);

    inline const Vector3 &GetLocalScale() const { return m_local_scale; }
    inline void SetLocalScale(const Vector3 &scale) 
    { 
        m_local_scale = scale;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Vector3 &GetGlobalScale() const { return m_spatial.m_transform.GetScale(); }
    void SetGlobalScale(const Vector3 &scale);

    inline const Transform &GetGlobalTransform() const { return m_spatial.m_transform; }

    inline void Move(const Vector3 &vec) { SetLocalTranslation(m_local_translation + vec); }
    inline void Scale(const Vector3 &vec) { SetLocalScale(m_local_scale * vec); }
    inline void Rotate(const Quaternion &rot) { SetLocalRotation(m_local_rotation * rot); }

    virtual void UpdateTransform();
    virtual void UpdateAABB();

    virtual float CalculateCameraDistance(Camera *camera) const;

    inline const BoundingBox &GetAABB() const { return m_spatial.m_aabb; }

    inline non_owning_ptr<Node> &GetParent() { return m_parent; }
    inline const non_owning_ptr<Node> &GetParent() const { return m_parent; }

    inline Material &GetMaterial() { return m_spatial.m_material; }
    inline const Material &GetMaterial() const { return m_spatial.m_material; }
    inline void SetMaterial(const Material &material) { m_spatial.m_material = material; }

    void AddChild(std::shared_ptr<Node> node);
    void RemoveChild(std::shared_ptr<Node> node);
    std::shared_ptr<Node> GetChild(size_t index) const;
    std::shared_ptr<Node> GetChild(const std::string &name) const;
    inline size_t NumChildren() const { return m_children.size(); }

    void AddChildAsync(std::shared_ptr<Node> node, NodeCallback_t on_added);

    void AddPending();
    void ClearPendingRemoval();

    template <typename T>
    std::shared_ptr<T> GetChild(size_t index) const
    {
        static_assert(std::is_base_of<Node, T>::value,
            "T must be a derived class of Node");
        return std::dynamic_pointer_cast<T>(GetChild(index));
    }

    template <typename T>
    std::shared_ptr<T> GetChild(const std::string &name) const
    {
        static_assert(std::is_base_of<Node, T>::value,
            "T must be a derived class of Node");
        return std::dynamic_pointer_cast<T>(GetChild(name));
    }

    void AddControl(std::shared_ptr<EntityControl> control);
    void RemoveControl(const std::shared_ptr<EntityControl> &control);
    inline size_t NumControls() const { return m_controls.size(); }
    inline std::shared_ptr<EntityControl> GetControl(size_t index) const
    {
        soft_assert_return(index < NumControls(), nullptr);

        return m_controls[index];
    }

    template <typename T>
    std::shared_ptr<T> GetControl(size_t index) const
    {
        static_assert(std::is_base_of<EntityControl, T>::value,
            "T must be a derived class of EntityControl");
        return std::dynamic_pointer_cast<T>(GetControl(index));
    }

    inline Spatial &GetSpatial() { return m_spatial; }
    inline const Spatial &GetSpatial() const { return m_spatial; }
    inline void SetSpatial(const Spatial &spatial)
    {
        m_spatial = spatial;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const std::shared_ptr<Renderable> &GetRenderable() const { return m_spatial.m_renderable; }
    inline void SetRenderable(const std::shared_ptr<Renderable> &renderable) { m_spatial.m_renderable = renderable; }

    inline bool PendingRemoval() const { return m_flags & PENDING_REMOVAL; }
    inline bool PendingAddition() const { return m_flags & PENDING_ADDITION; }

    virtual void Update(double dt);
    void UpdateControls(double dt);

    // assumes m_octant as NOT become invalidated.
    // will call InsertNode() and RemoveNode() methods,
    // removing the node from the current Octree and inserting it
    // into the new one.
    void SetOctant(non_owning_ptr<Octree> octant);

    virtual std::shared_ptr<Loadable> Clone() override;

#pragma region serialization
    FBOM_DEF_DESERIALIZER(loader, in, out) {
        using namespace fbom;

        std::string name;
        if (auto err = in->GetProperty("name").ReadString(name)) {
            return err;
        }

        Vector3 translation;
        if (auto err = in->GetProperty("local_translation").ReadArrayElements(FBOMFloat(), 3, (unsigned char*)&translation)) {
            return err;
        }

        Vector3 scale;
        if (auto err = in->GetProperty("local_scale").ReadArrayElements(FBOMFloat(), 3, (unsigned char*)&scale)) {
            return err;
        }

        Quaternion rotation;
        if (auto err = in->GetProperty("local_rotation").ReadArrayElements(FBOMFloat(), 4, (unsigned char*)&rotation)) {
            return err;
        }

        auto out_entity = std::make_shared<Node>(name);
        out = out_entity;

        out_entity->SetLocalTranslation(translation);
        out_entity->SetLocalScale(scale);
        out_entity->SetLocalRotation(rotation);

        for (auto &node : in->nodes) {
            ex_assert(node->deserialized_object != nullptr);

            FBOMDeserialized child = node->deserialized_object;

            if (child->GetLoadableType().IsOrExtends("ENTITY")) {
                auto child_entity = std::dynamic_pointer_cast<Node>(child);
                ex_assert(child_entity != nullptr);

                out_entity->AddChild(child_entity);

                continue;
            }

            if (child->GetLoadableType().IsOrExtends("CONTROL")) {
                auto control = std::dynamic_pointer_cast<EntityControl>(child);
                ex_assert(control != nullptr);

                out_entity->AddControl(control);

                continue;
            }

            if (child->GetLoadableType().IsOrExtends("RENDERABLE")) {
                auto renderable = std::dynamic_pointer_cast<Renderable>(child);
                ex_assert(renderable != nullptr);

                out_entity->SetRenderable(renderable);

                continue;
            }

            if (child->GetLoadableType().IsOrExtends("MATERIAL")) {
                auto material = std::dynamic_pointer_cast<Material>(child);
                ex_assert(material != nullptr);

                out_entity->SetMaterial(*material);

                continue;
            }

            return FBOMResult(FBOMResult::FBOM_ERR, std::string("Node does not know how to handle ") + node->m_object_type.name + " subnode");
        }

        return FBOMResult::FBOM_OK;
    }

    FBOM_DEF_SERIALIZER(loader, in, out)
    {
        // TODO: static data and instancing
        using namespace fbom;

        auto entity = dynamic_cast<Node*>(in);

        if (entity == nullptr) {
            return FBOMResult::FBOM_ERR;
        }

        out->SetProperty("name", FBOMString(), entity->GetName().size(), (void*)entity->m_name.data());
        out->SetProperty("local_translation", FBOMArray(FBOMFloat(), 3), (void*)&entity->m_local_translation);
        out->SetProperty("local_scale", FBOMArray(FBOMFloat(), 3), (void*)&entity->m_local_scale);
        out->SetProperty("local_rotation", FBOMArray(FBOMFloat(), 4), (void*)&entity->m_local_rotation);

        for (size_t i = 0; i < entity->NumChildren(); i++) {
            if (auto child = entity->GetChild(i)) {
                FBOMObject *child_object = out->AddChild(child->GetLoadableType());
                FBOMResult loader_result = loader->Serialize(child.get(), child_object);

                if (loader_result != FBOMResult::FBOM_OK) {
                    return loader_result;
                }
            }
        }

        for (size_t i = 0; i < entity->NumControls(); i++) {
            if (auto control = entity->GetControl(i)) {
                FBOMObject *control_object = out->AddChild(control->GetLoadableType());
                FBOMResult loader_result = loader->Serialize(control.get(), control_object);

                if (loader_result != FBOMResult::FBOM_OK) {
                    return loader_result;
                }
            }
        }

        if (auto renderable = entity->GetRenderable()) {
            FBOMObject *renderable_object = out->AddChild(renderable->GetLoadableType());
            FBOMResult loader_result = loader->Serialize(renderable.get(), renderable_object);

            if (loader_result != FBOMResult::FBOM_OK) {
                return loader_result;
            }
        }

        FBOMObject *material_object = out->AddChild(entity->GetMaterial().GetLoadableType());
        if (auto err = loader->Serialize(&entity->GetMaterial(), material_object)) {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
#pragma endregion serialization

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_name);
        hc.Add(m_flags);
        hc.Add(GetMaterial().GetHashCode());
        hc.Add(GetGlobalTransform().GetHashCode());
        hc.Add(intptr_t(GetRenderable().get())); // TODO: maybe make this calc hash code

        for (const auto &child : m_children) {
            if (child == nullptr) {
                continue;
            }

            hc.Add(child->GetHashCode());
        }

        return hc;
    }

protected:
    std::string m_name;

    Spatial m_spatial;

    //std::shared_ptr<Renderable> m_renderable;
    std::vector<std::shared_ptr<Node>> m_children;
    std::vector<std::shared_ptr<Node>> m_children_pending_removal;
    std::vector<PendingNode_t> m_children_pending_addition;
    std::vector<std::shared_ptr<EntityControl>> m_controls;

    int m_flags;
    bool m_aabb_affects_parent;
    Vector3 m_local_translation;
    Vector3 m_local_scale;
    Quaternion m_local_rotation;
    //Transform m_global_transform;
    //BoundingBox m_aabb;
    non_owning_ptr<Node> m_parent;
    //Material m_material;

    void SetTransformUpdateFlag();
    void SetAABBUpdateFlag();
    void SetPendingRemovalFlag();

    std::shared_ptr<Node> CloneImpl();

private:
    int m_octree_node_id;
    non_owning_ptr<Octree> m_octant;

    // assumes m_octant may have become invalidated in the meantime due to updates
    // to the octree leading up to this call.
    // does NOT call any methods on, nor deference `m_octant`.
    void SetOctantInternal(non_owning_ptr<Octree> octant);
    
    static int NodeId();
};
} // namespace hyperion

#endif
