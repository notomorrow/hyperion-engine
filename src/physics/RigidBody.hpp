/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/object/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/math/Transform.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <physics/PhysicsMaterial.hpp>

#include <core/Types.hpp>

#include <type_traits>

namespace hyperion {
namespace physics {

enum class PhysicsShapeType : uint32
{
    NONE,
    BOX,
    SPHERE,
    PLANE,
    CONVEX_HULL
};

HYP_CLASS(Abstract)
class PhysicsShape : public HypObjectBase
{
    HYP_OBJECT_BODY(PhysicsShape);

    PhysicsShape()
        : m_type(PhysicsShapeType::NONE)
    {
    }

protected:
    PhysicsShape(PhysicsShapeType type)
        : m_type(type)
    {
    }

public:
    ~PhysicsShape() = default;

    HYP_FORCE_INLINE PhysicsShapeType GetType() const
    {
        return m_type;
    }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_FORCE_INLINE void* GetHandle() const
    {
        return m_handle.Get();
    }

    /*! \brief Set the internal handle of the PhysicsShape. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE void SetHandle(UniquePtr<void>&& handle)
    {
        m_handle = std::move(handle);
    }

protected:
    PhysicsShapeType m_type;

    UniquePtr<void> m_handle;
};

HYP_CLASS()
class BoxPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(BoxPhysicsShape);

public:
    BoxPhysicsShape(const BoundingBox& aabb)
        : PhysicsShape(PhysicsShapeType::BOX),
          m_aabb(aabb)
    {
    }

    ~BoxPhysicsShape() = default;

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

protected:
    BoundingBox m_aabb;
};

HYP_CLASS()
class SpherePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(SpherePhysicsShape);

public:
    SpherePhysicsShape(const BoundingSphere& sphere)
        : PhysicsShape(PhysicsShapeType::SPHERE),
          m_sphere(sphere)
    {
    }

    ~SpherePhysicsShape() = default;

    HYP_FORCE_INLINE const BoundingSphere& GetSphere() const
    {
        return m_sphere;
    }

protected:
    BoundingSphere m_sphere;
};

HYP_CLASS()
class PlanePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(PlanePhysicsShape);

public:
    PlanePhysicsShape(const Vec4f& plane)
        : PhysicsShape(PhysicsShapeType::PLANE),
          m_plane(plane)
    {
    }

    ~PlanePhysicsShape() = default;

    HYP_FORCE_INLINE const Vec4f& GetPlane() const
    {
        return m_plane;
    }

protected:
    Vec4f m_plane;
};

HYP_CLASS()
class ConvexHullPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(ConvexHullPhysicsShape);

public:
    ConvexHullPhysicsShape(const Array<Vec3f>& vertices)
        : PhysicsShape(PhysicsShapeType::CONVEX_HULL)
    {
        m_vertices.Resize(vertices.Size() * 3);

        for (SizeType index = 0; index < vertices.Size(); index++)
        {
            m_vertices[index * 3] = vertices[index].x;
            m_vertices[index * 3 + 1] = vertices[index].y;
            m_vertices[index * 3 + 2] = vertices[index].z;
        }
    }

    ~ConvexHullPhysicsShape() = default;

    HYP_FORCE_INLINE const float* GetVertexData() const
    {
        return m_vertices.Data();
    }

    HYP_FORCE_INLINE SizeType NumVertices() const
    {
        return m_vertices.Size() / 3;
    }

protected:
    Array<float> m_vertices;
};

HYP_CLASS()
class HYP_API RigidBody final : public HypObjectBase
{
    HYP_OBJECT_BODY(RigidBody);

public:
    RigidBody();
    RigidBody(const PhysicsMaterial& physicsMaterial);
    RigidBody(const Handle<PhysicsShape>& shape, const PhysicsMaterial& physicsMaterial);

    RigidBody(const RigidBody& other) = delete;
    RigidBody& operator=(const RigidBody& other) = delete;
    ~RigidBody();

    /*! \brief Get the world-space transform of this RigidBody. */
    HYP_METHOD(Serialize, Property = "Transform")
    HYP_FORCE_INLINE const Transform& GetTransform() const
    {
        return m_transform;
    }

    HYP_METHOD(Serialize, Property = "Transform")
    HYP_FORCE_INLINE void SetTransform(const Transform& transform)
    {
        m_transform = transform;
    }

    HYP_METHOD(Serialize, Property = "Shape")
    HYP_FORCE_INLINE const Handle<PhysicsShape>& GetShape() const
    {
        return m_shape;
    }

    HYP_METHOD(Serialize, Property = "Shape")
    void SetShape(const Handle<PhysicsShape>& shape);

    HYP_FORCE_INLINE PhysicsMaterial& GetPhysicsMaterial()
    {
        return m_physicsMaterial;
    }

    HYP_FORCE_INLINE const PhysicsMaterial& GetPhysicsMaterial() const
    {
        return m_physicsMaterial;
    }

    void SetPhysicsMaterial(const PhysicsMaterial& physicsMaterial);

    HYP_METHOD(Serialize, Property = "IsKinematic")
    HYP_FORCE_INLINE bool IsKinematic() const
    {
        return m_isKinematic;
    }

    HYP_METHOD(Serialize, Property = "IsKinematic")
    HYP_FORCE_INLINE void SetIsKinematic(bool isKinematic)
    {
        m_isKinematic = isKinematic;
    }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_FORCE_INLINE void* GetHandle() const
    {
        return m_handle.Get();
    }

    /*! \brief Set the internal handle of the RigidBody. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE void SetHandle(UniquePtr<void>&& handle)
    {
        m_handle = std::move(handle);
    }

    HYP_METHOD()
    void ApplyForce(const Vec3f& force);

private:
    void Init() override;

    Transform m_transform;
    Handle<PhysicsShape> m_shape;
    PhysicsMaterial m_physicsMaterial;
    bool m_isKinematic;

    UniquePtr<void> m_handle;
};

} // namespace physics

using physics::BoxPhysicsShape;
using physics::ConvexHullPhysicsShape;
using physics::PhysicsShape;
using physics::PlanePhysicsShape;
using physics::RigidBody;
using physics::SpherePhysicsShape;

} // namespace hyperion

