/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/Float16.hpp>

#include <Rendering/Shared.hpp>

namespace Hyperion {

class Texture;
class Color;

// format helpers live with TextureDesc in Shared.hpp
namespace TextureUtils {

/// Replaces alpha in every mip past 0 with the fraction of mip 0 texels at or above \p alphaCutoff that it covers.
/// Cutout shaders dither minified samples on that fraction instead of testing it against the cutoff.
void BuildAlphaCoverageMips(const TextureDesc& desc, ByteBuffer& imageData, float alphaCutoff);

/// Linear average of mip 0 of a 2D texture. White where the format can't be read on the CPU.
Vec4f ComputeAverageColor(const TextureDesc& desc, ConstByteView imageData);

inline ubyte LinearToSRGB8(float value)
{
    value = MathUtil::Clamp(value, 0.0f, 1.0f);
    value = value <= 0.0031308f
        ? value * 12.92f
        : 1.055f * MathUtil::Pow(value, 1.0f / 2.4f) - 0.055f;

    return ubyte(MathUtil::Clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
}

ENGINE_API ByteBuffer ResampleToSRGB8(const Float16* src, Vec2u srcExtent, Vec2u dstExtent);

/// Mipmapped square sRGB texture of 2x2 checkerboard
ENGINE_API Handle<Texture> CreateCheckerTexture(
    Name name,
    uint32 textureSize,
    const Color& lightColor,
    const Color& darkColor);

} // namespace TextureUtils

} // namespace Hyperion
