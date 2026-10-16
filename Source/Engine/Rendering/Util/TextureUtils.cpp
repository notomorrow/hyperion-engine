/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Util/TextureUtils.hpp>
#include <Rendering/Texture.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Math/Color.hpp>

#include <Util/Img/Bitmap.hpp>

#include <utility>

namespace Hyperion {

namespace TextureUtils {

void BuildAlphaCoverageMips(const TextureDesc& desc, ByteBuffer& imageData, float alphaCutoff)
{
    const bool isRgba8 = desc.format == TextureFormat::RGBA8 || desc.format == TextureFormat::RGBA8_SRGB;

    if (!isRgba8 || !desc.HasStoredMips() || desc.GetMipExtent(0).z != 1 || alphaCutoff <= 0.0f || alphaCutoff > 1.0f)
    {
        return;
    }

    AssertDebug(imageData.Size() == desc.GetByteSize());

    const uint32 numMipLevels = desc.NumMips();
    const uint32 numArrayLayers = desc.NumArrayLayers();
    const uint32 cutoffByte = MathUtil::Clamp(uint32(MathUtil::Ceil(alphaCutoff * 255.0f)), 1u, 255u);

    const Vec3u baseExtent = desc.GetMipExtent(0);
    const size_t baseLayerSize = desc.GetMipByteSize(0);

    Array<float> coverage;
    Array<float> nextCoverage;

    for (uint32 layer = 0; layer < numArrayLayers; layer++)
    {
        const ubyte* baseLayerData = imageData.Data() + layer * baseLayerSize;

        uint32 width = baseExtent.x;
        uint32 height = baseExtent.y;

        coverage.ResizeUninitialized(size_t(width) * size_t(height));

        for (size_t pixelIndex = 0; pixelIndex < coverage.Size(); pixelIndex++)
        {
            coverage[pixelIndex] = baseLayerData[pixelIndex * 4 + 3] >= cutoffByte ? 1.0f : 0.0f;
        }

        for (uint32 mip = 1; mip < numMipLevels; mip++)
        {
            const Vec3u mipExtent = desc.GetMipExtent(uint8(mip));
            const size_t mipLayerSize = desc.GetMipByteSize(uint8(mip));

            ubyte* mipLayerData = imageData.Data() + desc.mipOffsets[mip - 1] + layer * mipLayerSize;

            nextCoverage.ResizeUninitialized(size_t(mipExtent.x) * size_t(mipExtent.y));

            for (uint32 y = 0; y < mipExtent.y; y++)
            {
                const uint32 sourceRow0 = MathUtil::Min(y * 2, height - 1) * width;
                const uint32 sourceRow1 = MathUtil::Min(y * 2 + 1, height - 1) * width;

                for (uint32 x = 0; x < mipExtent.x; x++)
                {
                    const uint32 sourceColumn0 = MathUtil::Min(x * 2, width - 1);
                    const uint32 sourceColumn1 = MathUtil::Min(x * 2 + 1, width - 1);

                    const float value = 0.25f * (coverage[sourceRow0 + sourceColumn0] + coverage[sourceRow0 + sourceColumn1]
                        + coverage[sourceRow1 + sourceColumn0] + coverage[sourceRow1 + sourceColumn1]);

                    const size_t pixelIndex = size_t(y) * mipExtent.x + x;

                    nextCoverage[pixelIndex] = value;
                    mipLayerData[pixelIndex * 4 + 3] = ubyte(value * 255.0f + 0.5f);
                }
            }

            std::swap(coverage, nextCoverage);

            width = mipExtent.x;
            height = mipExtent.y;
        }
    }
}

template <class ComponentType, bool IsSrgb = false>
static Vec4f ReadTexel(const ubyte* texel, uint32 numComponents)
{
    switch (numComponents)
    {
    case 1:
        return ConstPixelReference<ComponentType, 1, IsSrgb>(texel).GetRGBA();
    case 2:
        return ConstPixelReference<ComponentType, 2, IsSrgb>(texel).GetRGBA();
    case 3:
        return ConstPixelReference<ComponentType, 3, IsSrgb>(texel).GetRGBA();
    default:
        return ConstPixelReference<ComponentType, 4, IsSrgb>(texel).GetRGBA();
    }
}

Vec4f ComputeAverageColor(const TextureDesc& textureDesc, ConstByteView imageData)
{
    static constexpr uint32 maxSamplesPerAxis = 64;

    const TextureFormat format = textureDesc.format;

    const bool isSrgb = textureDesc.IsSrgb();
    const bool isUnorm8 = (format >= TextureFormat::R8 && format <= TextureFormat::BGRA8) || isSrgb;
    const bool isUnorm16 = format >= TextureFormat::R16 && format <= TextureFormat::RGBA16;
    const bool isFloat16 = format >= TextureFormat::R16F && format <= TextureFormat::RGBA16F;
    const bool isFloat32 = format >= TextureFormat::R32F && format <= TextureFormat::RGBA32F;

    const bool isBgr = (format >= TextureFormat::B8 && format <= TextureFormat::BGRA8)
        || (format >= TextureFormat::B8_SRGB && format <= TextureFormat::BGRA8_SRGB);

    const uint32 numComponents = TextureUtils::NumComponents(format);
    const uint32 bytesPerTexel = numComponents * TextureUtils::BytesPerComponent(format);

    if (textureDesc.type != TextureType::Texture2D
        || !(isUnorm8 || isUnorm16 || isFloat16 || isFloat32)
        || bytesPerTexel == 0 || numComponents > 4)
    {
        return Vec4f::One();
    }

    const Vec2u extent = textureDesc.extent.GetXY();

    if (imageData.Data() == nullptr || imageData.Size() < size_t(extent.x) * size_t(extent.y) * bytesPerTexel)
    {
        return Vec4f::One();
    }

    const Vec2u stride = { MathUtil::Max(extent.x / maxSamplesPerAxis, 1u), MathUtil::Max(extent.y / maxSamplesPerAxis, 1u) };

    Vec4f sum = Vec4f::Zero();
    uint32 numSamples = 0;

    for (uint32 y = stride.y / 2; y < extent.y; y += stride.y)
    {
        for (uint32 x = stride.x / 2; x < extent.x; x += stride.x)
        {
            const ubyte* texel = imageData.Data() + (size_t(y) * extent.x + x) * bytesPerTexel;

            if (isFloat16)
            {
                sum += ReadTexel<Float16>(texel, numComponents);
            }
            else if (isFloat32)
            {
                sum += ReadTexel<float>(texel, numComponents);
            }
            else if (isUnorm16)
            {
                // read as raw integers, alpha of 1 included where the format has none
                const Vec4f raw = ReadTexel<uint16>(texel, numComponents);

                sum += Vec4f(raw.x / 65535.0f, raw.y / 65535.0f, raw.z / 65535.0f, numComponents == 4 ? raw.w / 65535.0f : 1.0f);
            }
            else
            {
                sum += isSrgb ? ReadTexel<ubyte, true>(texel, numComponents) : ReadTexel<ubyte>(texel, numComponents);
            }

            ++numSamples;
        }
    }

    if (numSamples == 0)
    {
        return Vec4f::One();
    }

    Vec4f average = sum / float(numSamples);

    if (isBgr)
    {
        std::swap(average.x, average.z);
    }

    return average;
}

Handle<Texture> CreateCheckerTexture(
    Name name,
    uint32 textureSize,
    const Color& lightColor,
    const Color& darkColor)
{
    Assert(textureSize >= 2);

    const uint32 halfSize = textureSize / 2;

    TextureDesc textureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA8_SRGB,
        Vec3u { textureSize, textureSize, 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::Repeat
    };

    ByteBuffer imageBytes(size_t(textureSize) * size_t(textureSize) * 4);

    for (uint32 y = 0; y < textureSize; y++)
    {
        for (uint32 x = 0; x < textureSize; x++)
        {
            const bool isLight = (((x / halfSize) ^ (y / halfSize)) & 1) == 0;

            PixelReference<ubyte, 4, /* IsSrgb */ true>(imageBytes.Data() + (size_t(y) * textureSize + x) * 4)
                .SetRGBA(Vec4f(isLight ? lightColor : darkColor));
        }
    }

    Texture::GenerateMipmaps(textureDesc, imageBytes);

    Handle<Texture> texture = MakeHandle<Texture>(textureDesc, imageBytes.ToByteView());
    texture->SetName(name);
    InitObject(texture);

    return texture;
}

} // namespace TextureUtils

} // namespace Hyperion
