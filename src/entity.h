#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <vector>
#include <deque>
#include <memory>

#include "control.h"
#include "hash_code.h"
#include "asset/loadable.h"
#include "math/transform.h"
#include "rendering/renderable.h"
#include "rendering/material.h"

namespace hyperion {
class Camera;
class Entity : public Loadable {
public:
    enum UpdateFlags {
        UPDATE_TRANSFORM = 0x01,
        UPDATE_AABB = 0x02,
        PENDING_REMOVAL = 0x04
    };

    Entity(const std::string &name = "entity");
    virtual ~Entity();

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

    inline const Vector3 &GetGlobalTranslation() const { return m_global_transform.GetTranslation(); }
    void SetGlobalTranslation(const Vector3 &translation);

    inline const Quaternion &GetLocalRotation() const { return m_local_rotation; }
    inline void SetLocalRotation(const Quaternion &rotation) 
    { 
        m_local_rotation = rotation;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Quaternion &GetGlobalRotation() const { return m_global_transform.GetRotation(); }
    void SetGlobalRotation(const Quaternion &rotation);

    inline const Vector3 &GetLocalScale() const { return m_local_scale; }
    inline void SetLocalScale(const Vector3 &scale) 
    { 
        m_local_scale = scale;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Vector3 &GetGlobalScale() const { return m_global_transform.GetScale(); }
    void SetGlobalScale(const Vector3 &scale);

    inline const Transform &GetGlobalTransform() const { return m_global_transform; }

    inline void Move(const Vector3 &vec) { SetLocalTranslation(m_local_translation + vec); }
    inline void Scale(const Vector3 &vec) { SetLocalScale(m_local_scale * vec); }
    inline void Rotate(const Quaternion &rot) { SetLocalRotation(m_local_rotation * rot); }

    virtual void UpdateTransform();
    virtual void UpdateAABB();

    virtual float CalculateCameraDistance(Camera *camera) const;

    inline const BoundingBox &GetAABB() const { return m_aabb; }

    inline Entity *GetParent() const { return m_parent; }

    inline Material &GetMaterial() { return m_material; }
    inline const Material &GetMaterial() const { return m_material; }
    inline void SetMaterial(const Material &material) { m_material = material; }

    void AddChild(std::shared_ptr<Entity> entity);
    void RemoveChild(const std::shared_ptr<Entity> &entity);
    std::shared_ptr<Entity> GetChild(size_t index) const;
    std::shared_ptr<Entity> GetChild(const std::string &name) const;
    inline size_t NumChildren() const { return m_children.size(); }

    std::shared_ptr<Entity> GetChildPendingRemoval(size_t index) const;
    inline size_t NumChildrenPendingRemoval() const { return m_children_pending_removal.size(); }
    inline void ClearPendingRemoval()
    {
        m_children_pending_removal.clear();

        for (auto &child : m_children) {
            child->ClearPendingRemoval();
        }
    }

    template <typename T>
    std::shared_ptr<T> GetChild(size_t index) const
    {
        static_assert(std::is_base_of<Entity, T>::value,
            "T must be a derived class of Entity");
        return std::dynamic_pointer_cast<T>(GetChild(index));
    }

    template <typename T>
    std::shared_ptr<T> GetChild(const std::string &name) const
    {
        static_assert(std::is_base_of<Entity, T>::value,
            "T must be a derived class of Entity");
        return std::dynamic_pointer_cast<T>(GetChild(name));
    }

    void AddControl(std::shared_ptr<EntityControl> control);
    void RemoveControl(const std::shared_ptr<EntityControl> &control);
    inline std::shared_ptr<EntityControl> GetControl(size_t index) const { return m_controls[index]; }
    inline size_t NumControls() const { return m_controls.size(); }

    template <typename T>
    std::shared_ptr<T> GetControl(size_t index) const
    {
        static_assert(std::is_base_of<EntityControl, T>::value,
            "T must be a derived class of EntityControl");
        return std::dynamic_pointer_cast<T>(GetControl(index));
    }

    inline std::shared_ptr<Renderable> GetRenderable() const { return m_renderable; }
    inline void SetRenderable(const std::shared_ptr<Renderable> &renderable) { m_renderable = renderable; }

    inline bool PendingRemoval() const { return m_flags & PENDING_REMOVAL; }

    virtual void Update(double dt);
    void UpdateControls(double dt);

    virtual std::shared_ptr<Loadable> Clone();

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_name);
        hc.Add(m_flags);
        hc.Add(m_material.GetHashCode());
        hc.Add(m_global_transform.GetHashCode());
        hc.Add(intptr_t(m_renderable.get())); // TODO: maybe make this calc hash code

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
    std::shared_ptr<Renderable> m_renderable;
    std::vector<std::shared_ptr<Entity>> m_children;
    std::deque<std::shared_ptr<Entity>> m_children_pending_removal;
    std::vector<std::shared_ptr<EntityControl>> m_controls;

    int m_flags;
    bool m_aabb_affects_parent;
    Vector3 m_local_translation;
    Vector3 m_local_scale;
    Quaternion m_local_rotation;
    Transform m_global_transform;
    BoundingBox m_aabb;
    Entity *m_parent;
    Material m_material;

    void SetTransformUpdateFlag();
    void SetAABBUpdateFlag();
    void SetPendingRemovalFlag();

    std::shared_ptr<Entity> CloneImpl();
};
} // namespace hyperion

#endif
