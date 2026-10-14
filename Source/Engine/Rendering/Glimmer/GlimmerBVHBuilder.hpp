/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Glimmer/GlimmerMemory.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>
#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

struct GlimmerBVHNode
{
    float leftMin[3];
    uint32 leftIndex;   //!< Child node index, or first primitive when the child is a leaf

    float leftMax[3];
    uint32 leftCount;   //!< Leaf primitive count, 0 when the child is an interior node

    float rightMin[3];
    uint32 rightIndex;

    float rightMax[3];
    uint32 rightCount;
};

static_assert(sizeof(GlimmerBVHNode) == 64);

struct GlimmerBVHBuildParams
{
    uint32 minLeafSize = 2;
    uint32 maxLeafSize = 8;

    uint32 maxDepth = 32;

    float traversalCost = 1.0f;
};

/// @TODO Make namespace? since its only static functions
class ENGINE_API GlimmerBVHBuilder
{
public:
    /// Instantiated for GlimmerAllocator and DynamicAllocator
    template <class AllocatorType>
    static void Build(
        Span<const BoundingBox> primitiveBounds,
        const GlimmerBVHBuildParams& params,
        Array<GlimmerBVHNode, AllocatorType>& outNodes,
        Array<uint32, AllocatorType>& outPrimitiveOrder);

    /*! \brief Deepest root-to-leaf path, for sizing traversal stacks. */
    static uint32 CalculateDepth(Span<const GlimmerBVHNode> nodes);
};

} // namespace Hyperion
