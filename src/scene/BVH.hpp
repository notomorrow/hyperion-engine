/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>
#include <core/math/Vertex.hpp>
#include <core/math/Ray.hpp>

#include <core/Defines.hpp>

namespace hyperion {

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html

HYP_STRUCT()
struct BVHNode
{
    HYP_FIELD(Serialize)
    BoundingBox aabb;

    HYP_FIELD(Serialize)
    LinkedList<BVHNode> children;

    HYP_FIELD(Serialize)
    Array<Triangle, DynamicAllocator> triangles;

    HYP_FIELD(Serialize)
    bool isLeafNode = false;

    BVHNode() = default;

    BVHNode(const BoundingBox& aabb)
        : aabb(aabb),
          isLeafNode(true)
    {
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return aabb.IsValid() && aabb.IsFinite();
    }

    HYP_FORCE_INLINE void AddTriangle(const Triangle& triangle)
    {
        triangles.PushBack(triangle);
    }

    void Split(int maxDepth = 3)
    {
        Split_Internal(0, maxDepth);
    }

    HYP_NODISCARD RayTestResults TestRay(const Ray& ray) const
    {
        RayTestResults results;

        if (ray.TestAABB(aabb))
        {
            if (isLeafNode)
            {
                for (SizeType triangleIndex = 0; triangleIndex < triangles.Size(); triangleIndex++)
                {
                    const Triangle& triangle = triangles[triangleIndex];

                    ray.TestTriangle(triangle, triangleIndex, this, results);
                }
            }
            else
            {
                for (const BVHNode& node : children)
                {
                    results.Merge(node.TestRay(ray));
                }
            }
        }

        return results;
    }

private:
    void Split_Internal(int depth, int maxDepth)
    {
        if (isLeafNode)
        {
            if (triangles.Any())
            {
                if (depth < maxDepth)
                {
                    const Vec3f center = aabb.GetCenter();
                    const Vec3f extent = aabb.GetExtent();

                    const Vec3f& min = aabb.GetMin();
                    const Vec3f& max = aabb.GetMax();

                    for (int i = 0; i < 2; i++)
                    {
                        for (int j = 0; j < 2; j++)
                        {
                            for (int k = 0; k < 2; k++)
                            {
                                const Vec3f newMin = Vec3f(
                                    i == 0 ? min.x : center.x,
                                    j == 0 ? min.y : center.y,
                                    k == 0 ? min.z : center.z);

                                const Vec3f newMax = Vec3f(
                                    i == 0 ? center.x : max.x,
                                    j == 0 ? center.y : max.y,
                                    k == 0 ? center.z : max.z);

                                children.EmplaceBack(BoundingBox(newMin, newMax));
                            }
                        }
                    }

                    for (const Triangle& triangle : triangles)
                    {
                        for (BVHNode& node : children)
                        {
                            if (node.aabb.ContainsTriangle(triangle))
                            {
                                node.triangles.PushBack(triangle);
                            }
                        }
                    }

                    triangles.Clear();

                    isLeafNode = false;
                }
            }
        }

        for (BVHNode& node : children)
        {
            node.Split_Internal(depth + 1, maxDepth);
        }
    }
};

} // namespace hyperion
