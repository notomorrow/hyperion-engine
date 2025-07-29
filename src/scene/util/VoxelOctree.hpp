/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>

#include <core/utilities/Result.hpp>

#include <util/octree/Octree.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Material;
class EntityManager;

struct VoxelOctreeParams
{
    float voxelSize = 0.25f;
};

struct VoxelOctreeElement
{
    Handle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Transform transform;
    BoundingBox aabb;

    HYP_FORCE_INLINE bool operator==(const VoxelOctreeElement& other) const
    {
        return entity == other.entity
            && mesh == other.mesh
            && material == other.material
            && transform == other.transform
            && aabb == other.aabb;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(entity.GetHashCode());
        hc.Add(mesh.GetHashCode());
        hc.Add(material.GetHashCode());
        hc.Add(transform.GetHashCode());
        hc.Add(aabb);

        return hc;
    }
};

struct VoxelOctreeNode
{
    ObjId<Entity> entityId;
    ObjId<Mesh> meshId;
    Triangle triangle;

    HYP_FORCE_INLINE bool operator==(const VoxelOctreeNode& other) const
    {
        return entityId == other.entityId
            && meshId == other.meshId
            && triangle == other.triangle;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(entityId);
        hc.Add(meshId);
        hc.Add(triangle);

        return hc;
    }
};

using VoxelOctreeBuildResult = Result;

class HYP_API VoxelOctree : public OctreeBase<VoxelOctree, VoxelOctreeNode>
{
public:
    VoxelOctree()
    {
        m_flags = OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES;
    }

    VoxelOctree(const BoundingBox& aabb, VoxelOctree* parent = nullptr, uint8 index = 0)
        : OctreeBase(aabb, parent, index)
    {
        m_flags = OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES;
    }

    virtual ~VoxelOctree() override = default;

    VoxelOctreeBuildResult Build(const VoxelOctreeParams& params, EntityManager* entityManager);

private:
    virtual UniquePtr<VoxelOctree> CreateChildOctant(
        const BoundingBox& aabb,
        VoxelOctree* parent,
        uint8 index) override
    {
        return MakeUnique<VoxelOctree>(aabb, parent, index);
    }
};

} // namespace hyperion
