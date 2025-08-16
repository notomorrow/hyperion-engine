/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/font/FontAtlas.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <rendering/Texture.hpp>

#include <core/logging/Logger.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/io/ByteWriter.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

#pragma region FontAtlasTextureSet

FontAtlasTextureSet::~FontAtlasTextureSet()
{
    for (auto& atlas : atlases)
    {
        SafeDelete(std::move(atlas.second));
    }
}

void FontAtlasTextureSet::AddAtlas(uint32 pixelSize, Handle<Texture> texture, bool isMainAtlas)
{
    if (isMainAtlas)
    {
        AssertDebug(!mainAtlas.IsValid(), "Main atlas already set");
    }

    if (!texture.IsValid())
    {
        return;
    }

    atlases[pixelSize] = texture;

    if (isMainAtlas)
    {
        mainAtlas = texture;
    }
}

Handle<Texture> FontAtlasTextureSet::GetAtlasForPixelSize(uint32 pixelSize) const
{
    auto it = atlases.Find(pixelSize);

    if (it != atlases.End())
    {
        return it->second;
    }

    it = atlases.UpperBound(pixelSize);

    if (it != atlases.End())
    {
        return it->second;
    }

    return Handle<Texture> {};
}

#pragma endregion FontAtlasTextureSet

#pragma region FontAtlas

FontAtlas::FontAtlas(const FontAtlasTextureSet& atlasTextures, Vec2i cellDimensions, GlyphMetricsBuffer glyphMetrics, SymbolList symbolList)
    : m_atlasTextures(std::move(atlasTextures)),
      m_cellDimensions(cellDimensions),
      m_glyphMetrics(std::move(glyphMetrics)),
      m_symbolList(std::move(symbolList))
{
    Assert(m_symbolList.Size() != 0);

    for (auto& it : m_atlasTextures.atlases)
    {
        if (!it.second.IsValid())
        {
            continue;
        }

        InitObject(it.second);
    }
}

FontAtlas::FontAtlas(RC<FontFace> face)
    : m_face(std::move(face)),
      m_symbolList(GetDefaultSymbolList())
{
    Assert(m_symbolList.Size() != 0);

    // Each cell will be the same size at the largest symbol
    m_cellDimensions = FindMaxDimensions(m_face);
}

FontAtlas::~FontAtlas() = default;

FontAtlas::SymbolList FontAtlas::GetDefaultSymbolList()
{
    // first renderable symbol
    static constexpr uint32 charRangeStart = 33; // !

    // highest symbol in the ascii table
    static constexpr uint32 charRangeEnd = 126; // ~ + 1

    SymbolList symbolList;
    symbolList.Reserve(charRangeEnd - charRangeStart + 1);

    for (uint32 ch = charRangeStart; ch <= charRangeEnd; ch++)
    {
        symbolList.PushBack(ch);
    }

    return symbolList;
}

Result FontAtlas::RenderAtlasTextures()
{
    Assert(m_face != nullptr);

    if ((m_symbolList.Size() / s_symbolColumns) > s_symbolRows)
    {
        HYP_LOG(Font, Warning, "Symbol list size is greater than the allocated font atlas!");
    }

    m_glyphMetrics.Clear();
    m_glyphMetrics.Resize(m_symbolList.Size());

    const auto renderGlyphs = [&](float scale, bool isMainAtlas) -> Result
    {
        const Vec2i scaledExtent {
            MathUtil::Ceil<float, int>(float(m_cellDimensions.x) * scale),
            MathUtil::Ceil<float, int>(float(m_cellDimensions.y) * scale)
        };

        HYP_LOG(Font, Info, "Rendering font atlas for pixel size {}", scaledExtent.y);

        UniquePtr<FontAtlasBitmap> atlasBitmap = MakeUnique<FontAtlasBitmap>(uint32(scaledExtent.x * s_symbolColumns), uint32(scaledExtent.y * s_symbolRows));

        for (SizeType i = 0; i < m_symbolList.Size(); i++)
        {
            const FontFace::WChar symbol = m_symbolList[i];

            const Vec2i index { int(i % s_symbolColumns), int(i / s_symbolColumns) };
            const Vec2i offset = index * scaledExtent;

            if (index.y > s_symbolRows)
            {
                break;
            }

            Glyph glyph { m_face, m_face->GetGlyphIndex(symbol), scale };
            glyph.LoadMetrics();

            if (isMainAtlas)
            {
                m_glyphMetrics[i] = glyph.GetMetrics();
                m_glyphMetrics[i].imagePosition = offset;
            }

            TResult<UniquePtr<GlyphBitmap>> glyphRasterizeResult = glyph.Rasterize();

            if (glyphRasterizeResult.HasError())
            {
                HYP_LOG(Font, Error, "Failed to rasterize glyph for symbol '{}': {}", symbol, glyphRasterizeResult.GetError().GetMessage());

                return glyphRasterizeResult.GetError();
            }

            UniquePtr<GlyphBitmap> glyphBitmap = std::move(glyphRasterizeResult.GetValue());
            AssertDebug(glyphBitmap != nullptr);

            Rect<uint32> srcRect {
                0, 0,
                uint32(scaledExtent.x),
                uint32(scaledExtent.y)
            };

            Rect<uint32> dstRect {
                uint32(offset.x),
                uint32(offset.y),
                uint32(offset.x + scaledExtent.x),
                uint32(offset.y + scaledExtent.y)
            };

            atlasBitmap->Blit(*glyphBitmap, srcRect, dstRect);
        }

        atlasBitmap->FlipVertical();

        const TextureDesc atlasTextureDesc {
            TT_TEX2D,
            TF_RGBA8,
            Vec3u { atlasBitmap->GetWidth(), atlasBitmap->GetHeight(), 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE
        };

        ByteBuffer byteBuffer = atlasBitmap->GetUnpackedBytes(4);

        TextureData atlasTextureData {
            atlasTextureDesc,
            std::move(byteBuffer)
        };

        Handle<Texture> atlasTexture = CreateObject<Texture>(std::move(atlasTextureData));
        atlasTexture->SetName(NAME_FMT("FontAtlas_{}", scale));
        InitObject(atlasTexture);

        // Add initial atlas
        m_atlasTextures.AddAtlas(scaledExtent.y, std::move(atlasTexture), isMainAtlas);

        return {};
    };

    if (Result result = renderGlyphs(1.0f, true); result.HasError())
    {
        return result.GetError();
    }

    for (float i = 1.1f; i <= 3.0f; i += 0.1f)
    {
        if (auto result = renderGlyphs(i, false); result.HasError())
        {
            return result.GetError();
        }
    }

    return {};
}

Vec2i FontAtlas::FindMaxDimensions(const RC<FontFace>& face) const
{
    Vec2i highestDimensions = { 0, 0 };

    for (const auto& symbol : m_symbolList)
    {
        // Create the glyph but only load in the metadata
        Glyph glyph(face, face->GetGlyphIndex(symbol), 1.0f);
        glyph.LoadMetrics();

        // Get the size of each glyph
        Vec2i size = glyph.GetMax();

        if (size.x > highestDimensions.x)
        {
            highestDimensions.x = size.x;
        }

        if (size.y > highestDimensions.y)
        {
            highestDimensions.y = size.y;
        }
    }

    return highestDimensions;
}

Optional<const Glyph::Metrics&> FontAtlas::GetGlyphMetrics(FontFace::WChar symbol) const
{
    const auto it = m_symbolList.Find(symbol);

    if (it == m_symbolList.End())
    {
        return {};
    }

    const SizeType index = std::distance(m_symbolList.Begin(), it);
    Assert(index < m_glyphMetrics.Size(), "Index {} out of bounds of glyph metrics buffer, size: {}", index, m_glyphMetrics.Size());

    return m_glyphMetrics[index];
}

json::JSONValue FontAtlas::GenerateMetadataJSON(const String& outputDirectory) const
{
    json::JSONObject value;

    json::JSONObject atlasesValue;
    json::JSONObject pixelSizesValue;

    uint32 mainAtlasKey = uint32(-1);

    for (const auto& it : m_atlasTextures.atlases)
    {
        if (!it.second.IsValid())
        {
            continue;
        }

        if (m_atlasTextures.mainAtlas == it.second)
        {
            mainAtlasKey = it.first;
        }

        const String keyString = String::ToString(it.first);

        pixelSizesValue[keyString] = FilePath(outputDirectory) / ("atlas_" + keyString + ".bmp");
    }

    atlasesValue["pixel_sizes"] = std::move(pixelSizesValue);
    atlasesValue["main"] = json::JSONNumber(mainAtlasKey);

    value["atlases"] = std::move(atlasesValue);

    value["cell_dimensions"] = json::JSONObject {
        { "width", json::JSONNumber(m_cellDimensions.x) },
        { "height", json::JSONNumber(m_cellDimensions.y) }
    };

    json::JSONArray metricsArray;

    for (const Glyph::Metrics& metric : m_glyphMetrics)
    {
        metricsArray.PushBack(json::JSONObject {
            { "width", json::JSONNumber(metric.width) },
            { "height", json::JSONNumber(metric.height) },
            { "bearing_x", json::JSONNumber(metric.bearingX) },
            { "bearing_y", json::JSONNumber(metric.bearingY) },
            { "advance", json::JSONNumber(metric.advance) },
            { "image_position", json::JSONObject { { "x", json::JSONNumber(metric.imagePosition.x) }, { "y", json::JSONNumber(metric.imagePosition.y) } } } });
    }

    value["metrics"] = std::move(metricsArray);

    json::JSONArray symbolListArray;

    for (const auto& symbol : m_symbolList)
    {
        symbolListArray.PushBack(json::JSONNumber(symbol));
    }

    value["symbol_list"] = std::move(symbolListArray);

    return value;
}

#pragma endregion FontAtlas

}; // namespace hyperion
