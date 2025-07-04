/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESH_BUILDER_HPP
#define HYPERION_MESH_BUILDER_HPP

#include <core/containers/Array.hpp>

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Vertex.hpp>

#include <Types.hpp>

namespace hyperion {

class Mesh;

struct Quad
{
    Vertex vertices[4];

    constexpr Vertex& operator[](uint32 index)
    {
        return vertices[index];
    }

    constexpr const Vertex& operator[](uint32 index) const
    {
        return vertices[index];
    }
};

struct VoxelData
{
    Vec4f color;
};

struct Voxel
{
    BoundingBox aabb;
    bool filled = false;

    VoxelData data;
};

struct VoxelGrid
{
    Array<Voxel> voxels;
    Vec3u size;
    float voxelSize;

    VoxelGrid()
        : size(Vec3u::Zero()),
          voxelSize(0.25f)
    {
    }

    VoxelGrid(Vec3u size, float voxelSize = 0.25f)
        : size(size),
          voxelSize(voxelSize)
    {
        voxels.Resize(size.x * size.y * size.z);

        for (uint32 x = 0; x < size.x; x++)
        {
            for (uint32 y = 0; y < size.y; y++)
            {
                for (uint32 z = 0; z < size.z; z++)
                {
                    voxels[GetIndex(x, y, z)] = Voxel {
                        BoundingBox {
                            Vec3f(float(x), float(y), float(z)) * voxelSize,
                            Vec3f(float(x) + 1, float(y) + 1, float(z) + 1) * voxelSize }
                    };
                }
            }
        }
    }

    VoxelGrid(const VoxelGrid& other) = default;
    VoxelGrid& operator=(const VoxelGrid& other) = default;
    VoxelGrid(VoxelGrid&& other) noexcept = default;
    VoxelGrid& operator=(VoxelGrid&& other) noexcept = default;
    ~VoxelGrid() = default;

    uint32 GetIndex(uint32 x, uint32 y, uint32 z) const
    {
        return (x * size.y * size.z) + (y * size.z) + z;
    }

    const Voxel& GetVoxel(uint32 x, uint32 y, uint32 z) const
    {
        const uint32 index = GetIndex(x, y, z);
        Assert(index < voxels.Size(), "Voxel index out of bounds");

        return voxels[index];
    }

    void SetVoxel(uint32 x, uint32 y, uint32 z, VoxelData data)
    {
        const uint32 index = GetIndex(x, y, z);
        Assert(index < voxels.Size(), "Voxel index out of bounds");

        voxels[index].filled = true;
        voxels[index].data = data;
    }
};

class HYP_API MeshBuilder
{
    static const Array<Vertex> quadVertices;
    static const Array<uint32> quadIndices;
    static const Array<Vertex> cubeVertices;

public:
    static Handle<Mesh> Quad();
    static Handle<Mesh> Cube();
    static Handle<Mesh> NormalizedCubeSphere(uint32 numDivisions);

    static Handle<Mesh> ApplyTransform(const Mesh* mesh, const Transform& transform);
    static Handle<Mesh> Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform);
    static Handle<Mesh> Merge(const Mesh* a, const Mesh* b);

    static Handle<Mesh> BuildVoxelMesh(VoxelGrid voxelGrid);
};

} // namespace hyperion

#endif
