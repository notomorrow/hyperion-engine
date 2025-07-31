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

static BoundingBox SnapAabbToVoxel(const BoundingBox& aabb, float voxelSize)
{

    Vec3f extent = aabb.GetExtent();
    Vec3f newExtent = Vec3f(
        MathUtil::Ceil(extent.x / voxelSize) * voxelSize,
        MathUtil::Ceil(extent.y / voxelSize) * voxelSize,
        MathUtil::Ceil(extent.z / voxelSize) * voxelSize);

    BoundingBox newAabb = aabb;
    newAabb.SetExtent(newExtent);

    return newAabb;
}

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
    
    Array<Tuple<VoxelOctreeElement, MeshData*, ResourceHandle, const BVHNode*>> meshDatas;

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
        element.aabb = boundingBoxComponent.worldAabb;
//
//        tlas.Add(element, &meshComponent.mesh->GetBVH());
        
        if (!meshComponent.mesh->GetAsset())
        {
            continue;
        }
        
        ResourceHandle resourceHandle(*meshComponent.mesh->GetAsset()->GetResource());
        meshDatas.EmplaceBack(element, meshComponent.mesh->GetAsset()->GetMeshData(), resourceHandle, &meshComponent.mesh->GetBVH());
    }

    if (!newAabb.IsValid() || !newAabb.IsFinite())
    {
        return HYP_MAKE_ERROR(Error, "Invalid AABB, cannot build voxel octree");
    }
    
    newAabb = BoundingBox(Vec3f(-85.0f), Vec3f(85.0f));
    
    if (!OctreeBase::Rebuild(newAabb, true).first)
    {
        return HYP_MAKE_ERROR(Error, "Failed to rebuild voxel octree");
    }

//    TaskThreadPool pool("BuildVoxelOctreeTP", MathUtil::Min(meshDatas.Size(), 4));
//    pool.Start();
//
//    TaskBatch taskBatch;
//    taskBatch.pool = &pool;

//    Mutex mtx;

    Proc<bool(const VoxelOctreeElement&, const MeshData&, const BVHNode* bvhNode)> insertIntoOctree;

    insertIntoOctree = [&](const VoxelOctreeElement& element, const MeshData& meshData, const BVHNode* bvhNode) -> bool
    {
        if (meshData.desc.numIndices != 0)
        {
            Span<const uint32> meshIndices = Span<const uint32>(reinterpret_cast<const uint32*>(meshData.indexData.Data()), reinterpret_cast<const uint32*>(meshData.indexData.Data()) + (meshData.indexData.Size() / sizeof(uint32)));
            
            Assert(meshIndices.Size() % 3 == 0);

//            for (SizeType i = 0; i < meshIndices.Size(); i += 3)
//            {
//                Triangle triangle {
//                    Vertex(element.transform.GetMatrix() * meshData.vertexData[meshIndices[i]].position),
//                    Vertex(element.transform.GetMatrix() * meshData.vertexData[meshIndices[i + 1]].position),
//                    Vertex(element.transform.GetMatrix() * meshData.vertexData[meshIndices[i + 2]].position)
//                };
//                
//                BoundingBox triangleAabb = triangle.GetBoundingBox().Expand(0.002f);
//                
//                InsertResult insertResult = OctreeBase::Insert(
//                    VoxelOctreeNode { element.entity.Id(), element.mesh.Id(), triangle },
//                    triangleAabb, true);
//            }
            
            for (SizeType i = 0; i < bvhNode->triangles.Size(); i++)
            {
                Triangle triangle = element.transform.GetMatrix() * bvhNode->triangles[i];

                BoundingBox triangleAabb = triangle.GetBoundingBox().Expand(0.002f);

                InsertResult insertResult = OctreeBase::Insert(
                    VoxelOctreeNode { element.entity.Id(), element.mesh.Id(), triangle },
                    triangleAabb, true);
            }

            
            
//                    Mutex::Guard guard(mtx);

//            return IterationResult::CONTINUE;
            
//            // iterate triangles in batches so we don't hold exclusive access for too long
//            static constexpr uint32 batchSize = 36;
//            ForEachInBatches(meshIndices, (meshIndices.Size() + batchSize - 1) / batchSize, [&](Span<const uint32> indices)
//                {
//                    Assert(indices.Size() % 3 == 0);
//                
//                    Array<Triangle, FixedAllocator<batchSize / 3>> triangles;
//                    triangles.Resize(indices.Size() / 3);
//                
//                    Array<BoundingBox, FixedAllocator<batchSize / 3>> triangleAabbs;
//                    triangleAabbs.Resize(indices.Size() / 3);
//
//                    for (SizeType i = 0; i < indices.Size(); i += 3)
//                    {
//                        triangles[i / 3] = Triangle {
//                            Vertex(element.transform.GetMatrix() * meshData.vertexData[indices[i]].position),
//                            Vertex(element.transform.GetMatrix() * meshData.vertexData[indices[i + 1]].position),
//                            Vertex(element.transform.GetMatrix() * meshData.vertexData[indices[i + 2]].position)
//                        };
//                        
//                        triangleAabbs[i / 3] = triangles[i / 3].GetBoundingBox().Expand(0.2f);
//                    }
//
////                    Mutex::Guard guard(mtx);
//
//                    for (SizeType i = 0; i < triangles.Size(); i++)
//                    {
//                        InsertResult insertResult = OctreeBase::Insert(
//                            VoxelOctreeNode { element.entity.Id(), element.mesh.Id(), triangles[i] },
//                            triangleAabbs[i], true);
//                    }
//
//                    return IterationResult::CONTINUE;
//                });
        }

        for (const BVHNode& child : bvhNode->children)
        {
            if (!insertIntoOctree(element, meshData, &child))
            {
                return false;
            }
        }

        return true;
    };

    for (const auto& tup : meshDatas)
    {
        const VoxelOctreeElement& element = tup.GetElement<0>();
        const MeshData* meshData = tup.GetElement<1>();
        const BVHNode* bvhNode = tup.GetElement<3>();
        Assert(meshData != nullptr);

//        taskBatch.AddTask([&]()
//            {
                insertIntoOctree(element, *meshData, bvhNode);
//            });
    }

//    TaskSystem::GetInstance().EnqueueBatch(&taskBatch);
//    taskBatch.AwaitCompletion();
//
//    pool.Stop();

    return {};
}

} // namespace hyperion
