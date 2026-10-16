/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Name/Name.hpp>
#include <Core/Reflection/Handle.hpp>

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

/// Mipmapped square sRGB texture of 2x2 checkerboard
ENGINE_API Handle<Texture> CreateCheckerTexture(
    Name name,
    uint32 textureSize,
    const Color& lightColor,
    const Color& darkColor);

} // namespace TextureUtils

} // namespace Hyperion
