#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <vector>
#include <memory>

#include "control.h"
#include "asset/loadable.h"
#include "math/transform.h"
#include "rendering/renderable.h"

namespace apex {
class Entity : public Loadable {
public:
    enum UpdateFlags {
        UPDATE_TRANSFORM = 0x01
    };

    Entity(const std::string &name = "entity");
    virtual ~Entity();

    const std::string &GetName() const;
    void SetName(const std::string &name);

    const Vector3 &GetLocalTranslation() const;
    void SetLocalTranslation(const Vector3 &translation);
    const Vector3 &GetLocalScale() const;
    void SetLocalScale(const Vector3 &scale);
    const Quaternion &GetLocalRotation() const;
    void SetLocalRotation(const Quaternion &rotation);
    const Transform &GetGlobalTransform() const;

    void Move(const Vector3 &vec);
    void Scale(const Vector3 &vec);
    void Rotate(const Quaternion &rot);

    virtual void UpdateTransform();

    Entity *GetParent() const;

    void AddChild(std::shared_ptr<Entity> entity);
    void RemoveChild(const std::shared_ptr<Entity> &entity);
    std::shared_ptr<Entity> GetChild(size_t index) const;
    std::shared_ptr<Entity> GetChild(const std::string &name) const;
    size_t NumChildren() const;

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
    std::shared_ptr<EntityControl> GetControl(size_t index) const;
    size_t NumControls() const;

    template <typename T>
    std::shared_ptr<T> GetControl(size_t index) const
    {
        static_assert(std::is_base_of<EntityControl, T>::value,
            "T must be a derived class of EntityControl");
        return std::dynamic_pointer_cast<T>(GetControl(index));
    }

    std::shared_ptr<Renderable> GetRenderable() const;
    void SetRenderable(const std::shared_ptr<Renderable> &renderable);

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
    Entity *m_parent;

    void SetTransformUpdateFlag();
};
} // namespace apex

#endif