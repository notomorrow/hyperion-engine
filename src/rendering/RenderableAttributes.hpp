/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_RENDERABLE_ATTRIBUTES_HPP
#define HYPERION_RENDERABLE_ATTRIBUTES_HPP

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/ShaderManager.hpp>

#include <core/Defines.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

HYP_ENUM()
enum MaterialAttributeFlags : uint32
{
    MAF_NONE = 0x0,

    MAF_DEPTH_WRITE = 0x1,
    MAF_DEPTH_TEST = 0x2,
    MAF_ALPHA_DISCARD = 0x4
};

HYP_MAKE_ENUM_FLAGS(MaterialAttributeFlags)

HYP_STRUCT()
struct MaterialAttributes
{
    HYP_FIELD()
    ShaderDefinition shaderDefinition;

    HYP_FIELD()
    RenderBucket bucket = RB_OPAQUE;

    HYP_FIELD()
    FillMode fillMode = FM_FILL;

    HYP_FIELD()
    BlendFunction blendFunction = BlendFunction::None();

    HYP_FIELD()
    FaceCullMode cullFaces = FCM_BACK;

    HYP_FIELD()
    EnumFlags<MaterialAttributeFlags> flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST;

    HYP_FIELD()
    StencilFunction stencilFunction;

    HYP_FORCE_INLINE bool operator==(const MaterialAttributes& other) const
    {
        return shaderDefinition == other.shaderDefinition
            && bucket == other.bucket
            && fillMode == other.fillMode
            && blendFunction == other.blendFunction
            && cullFaces == other.cullFaces
            && flags == other.flags
            && stencilFunction == other.stencilFunction;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialAttributes& other) const
    {
        return shaderDefinition != other.shaderDefinition
            || bucket != other.bucket
            || fillMode != other.fillMode
            || blendFunction != other.blendFunction
            || cullFaces != other.cullFaces
            || flags != other.flags
            || stencilFunction != other.stencilFunction;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(shaderDefinition.GetHashCode());
        hc.Add(bucket);
        hc.Add(fillMode);
        hc.Add(blendFunction);
        hc.Add(cullFaces);
        hc.Add(flags);
        hc.Add(stencilFunction);

        return hc;
    }
};

HYP_STRUCT()
struct MeshAttributes
{
    HYP_FIELD(Property = "VertexAttributes", Serialize = true)
    VertexAttributeSet vertexAttributes = staticMeshVertexAttributes;

    HYP_FIELD(Property = "Topology", Serialize = true)
    Topology topology = TOP_TRIANGLES;

    HYP_FORCE_INLINE bool operator==(const MeshAttributes& other) const
    {
        return vertexAttributes == other.vertexAttributes
            && topology == other.topology;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(vertexAttributes);
        hc.Add(topology);

        return hc;
    }
};

class RenderableAttributeSet
{
    MeshAttributes m_meshAttributes;
    MaterialAttributes m_materialAttributes;
    uint32 m_overrideFlags;
    uint32 m_drawableLayer;

    mutable HashCode m_cachedHashCode;
    mutable bool m_needsHashCodeRecalculation;

public:
    RenderableAttributeSet(const MeshAttributes& meshAttributes = {}, const MaterialAttributes& materialAttributes = {}, uint32 overrideFlags = 0)
        : m_meshAttributes(meshAttributes),
          m_materialAttributes(materialAttributes),
          m_overrideFlags(overrideFlags),
          m_drawableLayer(0),
          m_needsHashCodeRecalculation(true)
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
        return m_materialAttributes.shaderDefinition;
    }

    HYP_FORCE_INLINE void SetShaderDefinition(const ShaderDefinition& shaderDefinition)
    {
        m_materialAttributes.shaderDefinition = shaderDefinition;
        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE const MeshAttributes& GetMeshAttributes() const
    {
        return m_meshAttributes;
    }

    HYP_FORCE_INLINE void SetMeshAttributes(const MeshAttributes& meshAttributes)
    {
        m_meshAttributes = meshAttributes;
        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE const MaterialAttributes& GetMaterialAttributes() const
    {
        return m_materialAttributes;
    }

    HYP_FORCE_INLINE void SetMaterialAttributes(const MaterialAttributes& materialAttributes)
    {
        m_materialAttributes = materialAttributes;
        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE uint32 GetOverrideFlags() const
    {
        return m_overrideFlags;
    }

    HYP_FORCE_INLINE void SetOverrideFlags(uint32 overrideFlags)
    {
        m_overrideFlags = overrideFlags;
        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE uint32 GetDrawableLayer() const
    {
        return m_drawableLayer;
    }

    HYP_FORCE_INLINE void SetDrawableLayer(uint32 drawableLayer)
    {
        m_drawableLayer = drawableLayer;
        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode();

            m_needsHashCodeRecalculation = false;
        }

        return m_cachedHashCode;
    }

private:
    void RecalculateHashCode() const
    {
        HashCode hc;
        hc.Add(m_meshAttributes.GetHashCode());
        hc.Add(m_materialAttributes.GetHashCode());
        hc.Add(m_overrideFlags);
        hc.Add(m_drawableLayer);

        m_cachedHashCode = hc;
    }
};

} // namespace hyperion

#endif
