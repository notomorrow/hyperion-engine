#ifndef HYPERION_V2_PHYSICS_RIGID_BODY_HPP
#define HYPERION_V2_PHYSICS_RIGID_BODY_HPP

#include <core/Base.hpp>
#include <core/lib/UniquePtr.hpp>
#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <math/BoundingSphere.hpp>
#include <physics/PhysicsMaterial.hpp>

#include <type_traits>

namespace hyperion::v2 {

class Engine;

} // namespace hyperion::v2

namespace hyperion::v2::physics {

enum class PhysicsShapeType
{
    NONE,
    BOX,
    SPHERE
};

class PhysicsShape
{
public:
    PhysicsShape(PhysicsShapeType type)
        : m_type(type)
    {
    }

    ~PhysicsShape() = default;

    PhysicsShapeType GetType() const
        { return m_type; }

    /*! \brief Return the handle specific to the physics engine in use */
    UniquePtr<void> &GetHandle()
        { return m_handle; }

    /*! \brief Return the handle specific to the physics engine in use */
    const UniquePtr<void> &GetHandle() const
        { return m_handle; }

    /*! \brief Set the internal handle of the PhysicsShape. Only to be used
        by a PhysicsAdapter. */
    void SetHandle(UniquePtr<void> &&handle)
        { m_handle = std::move(handle); }

protected:
    const PhysicsShapeType m_type;

    UniquePtr<void> m_handle;
};

class BoxPhysicsShape : public PhysicsShape
{
public:
    BoxPhysicsShape(const BoundingBox &aabb)
        : PhysicsShape(PhysicsShapeType::BOX)
    {
    }

    ~BoxPhysicsShape() = default;

    const BoundingBox &GetAABB() const
        { return m_aabb; }

protected:
    BoundingBox m_aabb;
};

class SpherePhysicsShape : public PhysicsShape
{
public:
    SpherePhysicsShape(const BoundingSphere &sphere)
        : PhysicsShape(PhysicsShapeType::SPHERE)
    {
    }

    ~SpherePhysicsShape() = default;

    const BoundingSphere &GetSphere() const
        { return m_sphere; }

protected:
    BoundingSphere m_sphere;
};

class RigidBody : public EngineComponentBase<STUB_CLASS(RigidBody)>
{
public:
    RigidBody(const PhysicsMaterial &physics_material)
        : EngineComponentBase(),
          m_physics_material(physics_material)
    {
    }

    RigidBody(UniquePtr<PhysicsShape> &&shape, const PhysicsMaterial &physics_material)
        : EngineComponentBase(),
          m_shape(std::move(shape)),
          m_physics_material(physics_material)
    {
    }

    RigidBody(const RigidBody &other) = delete;
    RigidBody &operator=(const RigidBody &other) = delete;
    ~RigidBody();

    void Init(Engine *engine);

    /*! \brief Get the world-space transform of this RigidBody.
        If changed, you will have to flag that the transform has changed,
        so that the physics engine's internal rigidbody will have its transform updated. */
    Transform &GetTransform()
        { return m_transform; }

    /*! \brief Get the world-space transform of this RigidBody. */
    const Transform &GetTransform() const
        { return m_transform; }
    
    void SetTransform(const Transform &transform)
        { m_transform = transform; }

    UniquePtr<PhysicsShape> &GetShape()
        { return m_shape; }

    const UniquePtr<PhysicsShape> &GetShape() const
        { return m_shape; }

    PhysicsMaterial &GetPhysicsMaterial()
        { return m_physics_material; }

    const PhysicsMaterial &GetPhysicsMaterial() const
        { return m_physics_material; }

    bool IsKinematic() const
        { return m_is_kinematic; }

    void SetIsKinematic(bool is_kinematic)
        { m_is_kinematic = is_kinematic; }

    /*! \brief Return the handle specific to the physics engine in use */
    UniquePtr<void> &GetHandle()
        { return m_handle; }

    /*! \brief Return the handle specific to the physics engine in use */
    const UniquePtr<void> &GetHandle() const
        { return m_handle; }

    /*! \brief Set the internal handle of the RigidBody. Only to be used
        by a PhysicsAdapter. */
    void SetHandle(UniquePtr<void> &&handle)
        { m_handle = std::move(handle); }

private:
    Transform m_transform;
    UniquePtr<PhysicsShape> m_shape;
    PhysicsMaterial m_physics_material;
    bool m_is_kinematic;

    UniquePtr<void> m_handle;
};

} // namespace hyperion::v2::physics

#endif