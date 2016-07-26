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
    void SetName(const std::string &str);

    const Vector3 &GetLocalTranslation() const;
    void SetLocalTranslation(const Vector3 &vec);
    const Vector3 &GetLocalScale() const;
    void SetLocalScale(const Vector3 &vec);
    const Quaternion &GetLocalRotation() const;
    void SetLocalRotation(const Quaternion &rot);
    const Transform &GetGlobalTransform() const;

    void Move(const Vector3 &vec);
    void Scale(const Vector3 &vec);
    void Rotate(const Quaternion &rot);

    void AddChild(const std::shared_ptr<Entity> &entity);
    void RemoveChild(const std::shared_ptr<Entity> &entity);
    std::shared_ptr<Entity> GetChild(size_t index) const;
    std::shared_ptr<Entity> GetChild(const std::string &name) const;
    size_t NumChildren() const;

    void AddControl(const std::shared_ptr<EntityControl> &control);
    void RemoveControl(const std::shared_ptr<EntityControl> &control);
    std::shared_ptr<EntityControl> GetControl(size_t index) const;
    size_t NumControls() const;

    std::shared_ptr<Renderable> GetRenderable() const;
    void SetRenderable(const std::shared_ptr<Renderable> &ren);

    virtual void Update(double dt);

protected:
    std::string name;
    std::shared_ptr<Renderable> renderable;
    std::vector<std::shared_ptr<Entity>> children;
    std::vector<std::shared_ptr<EntityControl>> controls;

    int flags;
    Vector3 local_translation;
    Vector3 local_scale;
    Quaternion local_rotation;
    Transform global_transform;
    Entity *parent;

    void SetTransformUpdateFlag();
};
}

#endif