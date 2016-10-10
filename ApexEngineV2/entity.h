#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <vector>
#include <memory>

#include "control.h"
#include "asset/loadable.h"
#include "math/transform.h"
#include "math/bounding_box.h"
#include "rendering/renderable.h"

namespace apex {

class Entity : public Loadable {
public:
    enum UpdateFlags {
        UPDATE_TRANSFORM = 0x01,
        UPDATE_AABB = 0x02
    };

    Entity(const std::string &name = "entity");
    virtual ~Entity();

    inline const std::string &GetName() const { return m_name; }
    inline void SetName(const std::string &name) { m_name = name; }

    inline const Vector3 &GetLocalTranslation() const { return m_local_translation; }
    inline void SetLocalTranslation(const Vector3 &translation) 
    { 
        m_local_translation = translation;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Vector3 &GetLocalScale() const { return m_local_scale; }
    inline void SetLocalScale(const Vector3 &scale) 
    { 
        m_local_scale = scale;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Quaternion &GetLocalRotation() const { return m_local_rotation; }
    inline void SetLocalRotation(const Quaternion &rotation) 
    { 
        m_local_rotation = rotation;
        SetTransformUpdateFlag();
        SetAABBUpdateFlag();
    }

    inline const Transform &GetGlobalTransform() const { return m_global_transform; }

    inline void Move(const Vector3 &vec) { SetLocalTranslation(m_local_translation + vec); }
    inline void Scale(const Vector3 &vec) { SetLocalScale(m_local_scale * vec); }
    inline void Rotate(const Quaternion &rot) { SetLocalRotation(m_local_rotation * rot); }

    virtual void UpdateTransform();
    virtual void UpdateAABB();

    inline const BoundingBox &GetAABB() const { return m_aabb; }

    inline Entity *GetParent() const { return m_parent; }

    void AddChild(std::shared_ptr<Entity> entity);
    void RemoveChild(const std::shared_ptr<Entity> &entity);
    std::shared_ptr<Entity> GetChild(size_t index) const;
    std::shared_ptr<Entity> GetChild(const std::string &name) const;
    inline size_t NumChildren() const { return m_children.size(); }

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

    virtual void Update(double dt);

protected:
    std::string m_name;
    std::shared_ptr<Renderable> m_renderable;
    std::vector<std::shared_ptr<Entity>> m_children;
    std::vector<std::shared_ptr<EntityControl>> m_controls;

    int m_flags;
    Vector3 m_local_translation;
    Vector3 m_local_scale;
    Quaternion m_local_rotation;
    Transform m_global_transform;
    BoundingBox m_aabb;
    Entity *m_parent;

    void SetTransformUpdateFlag();
    void SetAABBUpdateFlag();
};

} // namespace apex

#endif