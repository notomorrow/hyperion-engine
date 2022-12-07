#ifndef HYPERION_V2_RENDERABLE_ATTRIBUTES_H
#define HYPERION_V2_RENDERABLE_ATTRIBUTES_H

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Shader.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::FaceCullMode;
using renderer::Topology;
using renderer::FillMode;
using renderer::StencilState;
using renderer::VertexAttributeSet;
using renderer::BlendMode;

struct MaterialAttributes
{
    using MaterialFlags = UInt;

    enum MaterialFlagBits : MaterialFlags
    {
        RENDERABLE_ATTRIBUTE_FLAGS_NONE = 0x0,
        RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE = 0x1,
        RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST = 0x2
    };

    Bucket bucket = Bucket::BUCKET_OPAQUE;
    FillMode fill_mode = FillMode::FILL;
    BlendMode blend_mode = BlendMode::NONE;
    FaceCullMode cull_faces = FaceCullMode::BACK;
    MaterialFlags flags = RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE | RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;

    auto ToTuple() const
    {
        return std::make_tuple(
            bucket,
            fill_mode,
            blend_mode,
            cull_faces,
            flags
        );
    }

    bool operator==(const MaterialAttributes &other) const
    {
        return bucket == other.bucket
            && fill_mode == other.fill_mode
            && blend_mode == other.blend_mode
            && cull_faces == other.cull_faces
            && flags == other.flags;
    }

    bool operator<(const MaterialAttributes &other) const
    {
         return ToTuple() < other.ToTuple();
    }
};

struct MeshAttributes
{
    VertexAttributeSet vertex_attributes { renderer::static_mesh_vertex_attributes };
    Topology topology { Topology::TRIANGLES };

    auto ToTuple() const
    {
        return std::make_tuple(
            vertex_attributes,
            topology
        );
    }

    bool operator==(const MeshAttributes &other) const
    {
        return vertex_attributes == other.vertex_attributes
            && topology == other.topology;
    }

    bool operator<(const MeshAttributes &other) const
    {
         return ToTuple() < other.ToTuple();
    }
};

struct RenderableAttributeSet
{
    ID<Shader> shader_id;
    MeshAttributes mesh_attributes;
    MaterialAttributes material_attributes;
    StencilState stencil_state { };

    // auto ToTuple() const
    // {
    //     return std::tie(
    //         bucket,
    //         shader_id,
    //         vertex_attributes,
    //         topology,
    //         fill_mode,
    //         cull_faces,
    //         flags,
    //         stencil_state
    //     );
    // }

    RenderableAttributeSet() = default;

    RenderableAttributeSet(
        const MeshAttributes &mesh_attributes,
        const MaterialAttributes &material_attributes,
        ID<Shader> shader_id = Shader::empty_id
    ) : mesh_attributes(mesh_attributes),
        material_attributes(material_attributes),
        shader_id(shader_id)
    {
    }

    RenderableAttributeSet(const RenderableAttributeSet &other) = default;
    RenderableAttributeSet &operator=(const RenderableAttributeSet &other) = default;
    RenderableAttributeSet(RenderableAttributeSet &&other) noexcept = default;
    RenderableAttributeSet &operator=(RenderableAttributeSet &&other) noexcept = default;
    ~RenderableAttributeSet() = default;

    auto ToTuple() const
    {
        return std::make_tuple(
            shader_id,
            mesh_attributes,
            material_attributes,
            stencil_state
        );
    }

    bool operator==(const RenderableAttributeSet &other) const
    {
        return shader_id == other.shader_id
            && mesh_attributes == other.mesh_attributes
            && material_attributes == other.material_attributes
            && stencil_state == other.stencil_state;
    }

    bool operator<(const RenderableAttributeSet &other) const
    {
         return ToTuple() < other.ToTuple();
    }
};


} // namespace hyperion::v2

#endif
