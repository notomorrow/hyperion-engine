/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_RENDERABLE_ATTRIBUTES_HPP
#define HYPERION_RENDERABLE_ATTRIBUTES_HPP

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Shader.hpp>

#include <core/Defines.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

using renderer::FaceCullMode;
using renderer::FillMode;
using renderer::Topology;

using renderer::StencilCompareOp;
using renderer::StencilFunction;
using renderer::StencilOp;

using renderer::BlendFunction;
using renderer::BlendModeFactor;

enum class MaterialAttributeFlags : uint32
{
    NONE = 0x0,
    DEPTH_WRITE = 0x1,
    DEPTH_TEST = 0x2
};

HYP_MAKE_ENUM_FLAGS(MaterialAttributeFlags)

HYP_STRUCT()

struct MaterialAttributes
{
    HYP_FIELD()
    ShaderDefinition shader_definition;

    HYP_FIELD()
    Bucket bucket = Bucket::BUCKET_OPAQUE;

    HYP_FIELD()
    FillMode fill_mode = FillMode::FILL;

    HYP_FIELD()
    BlendFunction blend_function = BlendFunction::None();

    HYP_FIELD()
    FaceCullMode cull_faces = FaceCullMode::BACK;

    HYP_FIELD()
    EnumFlags<MaterialAttributeFlags> flags = MaterialAttributeFlags::DEPTH_WRITE | MaterialAttributeFlags::DEPTH_TEST;

    HYP_FIELD()
    StencilFunction stencil_function;

    HYP_FORCE_INLINE bool operator==(const MaterialAttributes& other) const
    {
        return shader_definition == other.shader_definition
            && bucket == other.bucket
            && fill_mode == other.fill_mode
            && blend_function == other.blend_function
            && cull_faces == other.cull_faces
            && flags == other.flags
            && stencil_function == other.stencil_function;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialAttributes& other) const
    {
        return shader_definition != other.shader_definition
            || bucket != other.bucket
            || fill_mode != other.fill_mode
            || blend_function != other.blend_function
            || cull_faces != other.cull_faces
            || flags != other.flags
            || stencil_function != other.stencil_function;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(shader_definition.GetHashCode());
        hc.Add(bucket);
        hc.Add(fill_mode);
        hc.Add(blend_function);
        hc.Add(cull_faces);
        hc.Add(flags);
        hc.Add(stencil_function);

        return hc;
    }
};

HYP_STRUCT()

struct MeshAttributes
{
    HYP_FIELD(Property = "VertexAttributes", Serialize = true)
    VertexAttributeSet vertex_attributes = static_mesh_vertex_attributes;

    HYP_FIELD(Property = "Topology", Serialize = true)
    Topology topology = Topology::TRIANGLES;

    HYP_FORCE_INLINE bool operator==(const MeshAttributes& other) const
    {
        return vertex_attributes == other.vertex_attributes
            && topology == other.topology;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(vertex_attributes);
        hc.Add(topology);

        return hc;
    }
};

class RenderableAttributeSet
{
    MeshAttributes m_mesh_attributes;
    MaterialAttributes m_material_attributes;
    uint32 m_override_flags;
    uint32 m_drawable_layer;

    mutable HashCode m_cached_hash_code;
    mutable bool m_needs_hash_code_recalculation;

public:
    RenderableAttributeSet(const MeshAttributes& mesh_attributes = {}, const MaterialAttributes& material_attributes = {}, uint32 override_flags = 0)
        : m_mesh_attributes(mesh_attributes),
          m_material_attributes(material_attributes),
          m_override_flags(override_flags),
          m_drawable_layer(0),
          m_needs_hash_code_recalculation(true)
    {
    }

    RenderableAttributeSet(const RenderableAttributeSet& other) = default;
    RenderableAttributeSet& operator=(const RenderableAttributeSet& other) = default;
    RenderableAttributeSet(RenderableAttributeSet&& other) noexcept = default;
    RenderableAttributeSet& operator=(RenderableAttributeSet&& other) noexcept = default;
    ~RenderableAttributeSet() = default;

    HYP_FORCE_INLINE bool operator==(const RenderableAttributeSet& other) const
    {
        return GetHashCode() == other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator!=(const RenderableAttributeSet& other) const
    {
        return GetHashCode() != other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator<(const RenderableAttributeSet& other) const
    {
        return GetHashCode().Value() < other.GetHashCode().Value();
    }

    HYP_FORCE_INLINE const ShaderDefinition& GetShaderDefinition() const
    {
        return m_material_attributes.shader_definition;
    }

    HYP_FORCE_INLINE void SetShaderDefinition(const ShaderDefinition& shader_definition)
    {
        m_material_attributes.shader_definition = shader_definition;
        m_needs_hash_code_recalculation = true;
    }

    HYP_FORCE_INLINE const MeshAttributes& GetMeshAttributes() const
    {
        return m_mesh_attributes;
    }

    HYP_FORCE_INLINE void SetMeshAttributes(const MeshAttributes& mesh_attributes)
    {
        m_mesh_attributes = mesh_attributes;
        m_needs_hash_code_recalculation = true;
    }

    HYP_FORCE_INLINE const MaterialAttributes& GetMaterialAttributes() const
    {
        return m_material_attributes;
    }

    HYP_FORCE_INLINE void SetMaterialAttributes(const MaterialAttributes& material_attributes)
    {
        m_material_attributes = material_attributes;
        m_needs_hash_code_recalculation = true;
    }

    HYP_FORCE_INLINE uint32 GetOverrideFlags() const
    {
        return m_override_flags;
    }

    HYP_FORCE_INLINE void SetOverrideFlags(uint32 override_flags)
    {
        m_override_flags = override_flags;
        m_needs_hash_code_recalculation = true;
    }

    HYP_FORCE_INLINE uint32 GetDrawableLayer() const
    {
        return m_drawable_layer;
    }

    HYP_FORCE_INLINE void SetDrawableLayer(uint32 drawable_layer)
    {
        m_drawable_layer = drawable_layer;
        m_needs_hash_code_recalculation = true;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_needs_hash_code_recalculation)
        {
            RecalculateHashCode();

            m_needs_hash_code_recalculation = false;
        }

        return m_cached_hash_code;
    }

private:
    void RecalculateHashCode() const
    {
        HashCode hc;
        hc.Add(m_mesh_attributes.GetHashCode());
        hc.Add(m_material_attributes.GetHashCode());
        hc.Add(m_override_flags);
        hc.Add(m_drawable_layer);

        m_cached_hash_code = hc;
    }
};

} // namespace hyperion

#endif
