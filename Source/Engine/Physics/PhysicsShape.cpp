/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 * */

#include <Physics/PhysicsShape.hpp>

#include <Rendering/Vertex.hpp>

#include <Core/Logging/Logger.hpp>

#include <PhysicsShape.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Physics);

#pragma region ConvexHullPhysicsShape

ConvexHullPhysicsShape::ConvexHullPhysicsShape(Name name, const VertexArrayView& vertexData)
    : PhysicsShape(name, PhysicsShapeType::ConvexHull)
{
    // Must only have position component
    Assert(vertexData.layoutDesc == StaticVertexInputLayout<VT_Position>);
    
    if (vertexData.vertexCount > 0)
    {
        AllocateBlobData(m_vertexData, vertexData.floatData, vertexData.vertexCount * sizeof(float) * 3, alignof(float));
    }
}

ConvexHullPhysicsShape::~ConvexHullPhysicsShape()
{
    FreeBlobData(m_vertexData);
}

void ConvexHullPhysicsShape::SetVertexData(const struct VertexArrayView& vertexData)
{
    FreeBlobData(m_vertexData);

    if (vertexData.vertexCount > 0)
    {
        AllocateBlobData(m_vertexData, vertexData.floatData, vertexData.vertexCount * sizeof(float) * 3, alignof(float));
    }

    MarkDirty();
}

#pragma endregion ConvexHullPhysicsShape

#pragma region HeightFieldPhysicsShape

HeightFieldPhysicsShape::HeightFieldPhysicsShape(Name name, Span<const float> heights, uint32 numSamplesXZ)
    : PhysicsShape(name, PhysicsShapeType::HeightField)
{
    SetHeights(heights, numSamplesXZ);
}

void HeightFieldPhysicsShape::SetHeights(Span<const float> heights, uint32 numSamplesXZ)
{
    if (heights.Size() != size_t(numSamplesXZ) * size_t(numSamplesXZ))
    {
        HYP_LOG(Physics, Warning, "HeightFieldPhysicsShape '{}' expected {}x{} height samples but got {}; update ignored",
            GetName(), numSamplesXZ, numSamplesXZ, heights.Size());

        return;
    }

    m_heights.Resize(heights.Size());

    for (uint32 i = 0; i < heights.Size(); i++)
    {
        m_heights[i] = heights[i];
    }

    m_numSamples = numSamplesXZ;

    MarkDirty();
}

#pragma endregion HeightFieldPhysicsShape

} // namespace Hyperion
