/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_MESH_COMPONENT_HPP
#define HYPERION_ECS_MESH_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/utilities/UserData.hpp>

#include <scene/animation/Skeleton.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderProxy.hpp>

namespace hyperion {

struct RenderProxy;

using MeshComponentFlags = uint32;

enum MeshComponentFlagBits : MeshComponentFlags
{
    MESH_COMPONENT_FLAG_NONE    = 0x0,
    MESH_COMPONENT_FLAG_DIRTY   = 0x1
};

using MeshComponentUserData = UserData<sizeof(Vec4u)>;

struct MeshComponent
{
    Handle<Mesh>            mesh;
    Handle<Material>        material;
    Handle<Skeleton>        skeleton;

    uint32                  num_instances = 1;

    RC<RenderProxy>         proxy;
    MeshComponentFlags      flags = MESH_COMPONENT_FLAG_DIRTY;

    Matrix4                 previous_model_matrix;

    MeshComponentUserData   user_data;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(mesh);
        hash_code.Add(material);
        hash_code.Add(skeleton);
        hash_code.Add(num_instances);

        return hash_code;
    }
};

static_assert(sizeof(MeshComponent) == 112, "MeshComponent size must match C# struct size");

} // namespace hyperion

#endif