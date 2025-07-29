/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/util/VoxelOctree.hpp>
#include <scene/Entity.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <scene/BVH.hpp>

#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

class VoxelOctreeBlas
{
public:
    VoxelOctreeBlas(const VoxelOctreeElement& element, const BVHNode* bvh)
        : m_element(element),
          m_root(bvh)
    {
        Assert(m_root != nullptr);
    }

    VoxelOctreeBlas(const VoxelOctreeBlas& other) = delete;
    VoxelOctreeBlas& operator=(const VoxelOctreeBlas& other) = delete;

    VoxelOctreeBlas(VoxelOctreeBlas&& other) noexcept
        : m_element(other.m_element),
          m_root(other.m_root)
    {
        other.m_root = nullptr;
    }

    VoxelOctreeBlas& operator=(VoxelOctreeBlas&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_element = std::move(other.m_element);
        m_root = std::move(other.m_root);

        other.m_root = nullptr;

        return *this;
    }

    ~VoxelOctreeBlas() = default;

    HYP_FORCE_INLINE const VoxelOctreeElement& GetElement() const
    {
        return m_element;
    }

    HYP_FORCE_INLINE const BVHNode* GetRoot() const
    {
        return m_root;
    }

private:
    VoxelOctreeElement m_element;
    const BVHNode* m_root;
};

struct VoxelOctreeTlas
{
    HYP_FORCE_INLINE const Transform& GetTransform() const
    {
        return Transform::identity;
    }

    HYP_FORCE_INLINE const Array<VoxelOctreeBlas>& GetAccelerationStructures() const
    {
        return m_accelerationStructures;
    }

    void Add(const VoxelOctreeElement& element, const BVHNode* bvh)
    {
        m_accelerationStructures.EmplaceBack(element, bvh);
    }

    void RemoveAll()
    {
        m_accelerationStructures.Clear();
    }

private:
    Array<VoxelOctreeBlas> m_accelerationStructures;
};

VoxelOctreeBuildResult VoxelOctree::Build(const VoxelOctreeParams& params, EntityManager* entityManager)
{
    Assert(entityManager != nullptr);

    if (params.voxelSize < 0.001)
    {
        return HYP_MAKE_ERROR(Error, "Voxel size must be greater than 0.001");
    }

    OctreeBase::Clear();

    BoundingBox newAabb = BoundingBox::Empty();

    // Build bvh structure
    VoxelOctreeTlas tlas;

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent] : entityManager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (!meshComponent.mesh.IsValid())
        {
            continue;
        }

        if (!meshComponent.material.IsValid())
        {
            continue;
        }

        // Only process opaque and translucent materials
        if (meshComponent.material->GetBucket() != RB_OPAQUE && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            continue;
        }

        if (!meshComponent.mesh->GetBVH().IsValid())
        {
            HYP_LOG_ONCE(Octree, Warning, "No valid BVH for mesh {} (ID: {}) on entity {}, skipping.", meshComponent.mesh->GetName(), meshComponent.mesh->Id(), entity->Id());

            continue;
        }

        newAabb = newAabb.Union(boundingBoxComponent.worldAabb);

        VoxelOctreeElement element {};
        element.entity = entity->HandleFromThis();
        element.mesh = meshComponent.mesh;
        element.material = meshComponent.material;
        element.transform = transformComponent.transform;

        tlas.Add(element, &meshComponent.mesh->GetBVH());
    }

    if (!newAabb.IsValid() || !newAabb.IsFinite())
    {
        return HYP_MAKE_ERROR(Error, "Invalid AABB, cannot build voxel octree");
    }

    // expand AABB to fit voxel size
    Vec3f extent = newAabb.GetExtent();
    Vec3f newExtent = Vec3f(
        MathUtil::Ceil(extent.x / params.voxelSize) * params.voxelSize,
        MathUtil::Ceil(extent.y / params.voxelSize) * params.voxelSize,
        MathUtil::Ceil(extent.z / params.voxelSize) * params.voxelSize);

    newAabb.SetExtent(newExtent);

    if (!OctreeBase::Rebuild(newAabb, true).first)
    {
        return HYP_MAKE_ERROR(Error, "Failed to rebuild voxel octree");
    }

    TaskThreadPool pool("BuildVoxelOctreeTP", MathUtil::Min(tlas.GetAccelerationStructures().Size(), 10));
    pool.Start();

    TaskBatch taskBatch;
    taskBatch.pool = &pool;

    Mutex mtx;

    Proc<bool(const VoxelOctreeElement&, const BVHNode&)> insertBvhIntoOctree;

    insertBvhIntoOctree = [&](const VoxelOctreeElement& element, const BVHNode& bvh) -> bool
    {
        if (!bvh.IsValid())
        {
            return false;
        }

        if (bvh.triangles.Any())
        {
            BoundingBox bvhAabb = bvh.aabb;
            // transform bvh aabb into world space
            bvhAabb = element.transform * bvhAabb;
            // root of Octree is zero so we don't need to transform it relatively

            // iterate triangles in batches so we don't hold exclusive access for too long
            ForEachInBatches(bvh.triangles, (bvh.triangles.Size() + 255) / 256, [&](Span<const Triangle> triangles)
                {
                    Array<BoundingBox> triangleAabbs;
                    triangleAabbs.Resize(triangles.Size());

                    for (SizeType i = 0; i < triangles.Size(); i++)
                    {
                        triangleAabbs[i] = element.transform * triangles[i].GetBoundingBox();
                    }

                    Mutex::Guard guard(mtx);

                    for (SizeType triangleIndex = 0; triangleIndex < triangles.Size(); triangleIndex++)
                    {
                        OctreeBase::Insert(
                            VoxelOctreeNode { element.entity.Id(), triangles[triangleIndex] },
                            triangleAabbs[triangleIndex], true);
                    }

                    return IterationResult::CONTINUE;
                });
        }

        for (const BVHNode& child : bvh.children)
        {
            if (!insertBvhIntoOctree(element, child))
            {
                return false;
            }
        }

        return true;
    };

    for (const VoxelOctreeBlas& blas : tlas.GetAccelerationStructures())
    {
        // Insert bvh root and leaf nodes into the octree
        const BVHNode& root = *blas.GetRoot();

        taskBatch.AddTask([&]()
            {
                insertBvhIntoOctree(blas.GetElement(), root);
            });
    }

    TaskSystem::GetInstance().EnqueueBatch(&taskBatch);
    taskBatch.AwaitCompletion();

    pool.Stop();

    return {};
}

} // namespace hyperion