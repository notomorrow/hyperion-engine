/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/font/FontAtlas.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <scene/Texture.hpp>

#include <core/logging/Logger.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

#pragma region Render commands

struct RENDER_COMMAND(FontAtlas_RenderCharacter)
    : RenderCommand
{
    Handle<Texture> atlasTexture;
    Handle<Texture> glyphTexture;
    Vec2i location;
    Vec2i cellDimensions;

    RENDER_COMMAND(FontAtlas_RenderCharacter)(const Handle<Texture>& atlasTexture, const Handle<Texture>& glyphTexture, Vec2i location, Vec2i cellDimensions)
        : atlasTexture(atlasTexture),
          glyphTexture(glyphTexture),
          location(location),
          cellDimensions(cellDimensions)
    {
    }

    virtual ~RENDER_COMMAND(FontAtlas_RenderCharacter)() override
    {
        atlasTexture->GetRenderResource().DecRef();
        glyphTexture->GetRenderResource().DecRef();
    }

    virtual RendererResult operator()() override
    {
        SingleTimeCommands commands;

        const ImageRef& image = glyphTexture->GetRenderResource().GetImage();
        AssertThrow(image.IsValid());

        const Vec3u& extent = image->GetExtent();

        Rect<uint32> srcRect {
            0, 0,
            extent.x,
            extent.y
        };

        Rect<uint32> destRect {
            uint32(location.x),
            uint32(location.y),
            uint32(location.x + extent.x),
            uint32(location.y + extent.y)
        };

        AssertThrowMsg(cellDimensions.x >= extent.x, "Cell width (%u) is less than glyph width (%u)", cellDimensions.x, extent.x);
        AssertThrowMsg(cellDimensions.y >= extent.y, "Cell height (%u) is less than glyph height (%u)", cellDimensions.y, extent.y);

        commands.Push([&](CmdList& cmd)
            {
                // put src image in state for copying from
                cmd.Add<InsertBarrier>(image, RS_COPY_SRC);

                // put dst image in state for copying to
                cmd.Add<InsertBarrier>(atlasTexture->GetRenderResource().GetImage(), RS_COPY_DST);

                cmd.Add<BlitRect>(image, atlasTexture->GetRenderResource().GetImage(), srcRect, destRect);
            });

        return commands.Execute();
    }
};

#pragma endregion Render commands

#pragma region FontAtlasTextureSet

FontAtlasTextureSet::~FontAtlasTextureSet()
{
    for (auto& atlas : atlases)
    {
        g_safeDeleter->SafeRelease(std::move(atlas.second));
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
    AssertThrow(m_symbolList.Size() != 0);

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
    AssertThrow(m_symbolList.Size() != 0);

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
    AssertThrow(m_face != nullptr);

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
            const auto& symbol = m_symbolList[i];

            const Vec2i index { int(i % s_symbolColumns), int(i / s_symbolColumns) };
            const Vec2i offset = index * scaledExtent;

            if (index.y > s_symbolRows)
            {
                break;
            }

            Glyph glyph { m_face, m_face->GetGlyphIndex(symbol), scale };

            if (isMainAtlas)
            {
                m_glyphMetrics[i] = glyph.GetMetrics();
                m_glyphMetrics[i].imagePosition = offset;

                AssertDebug(m_glyphMetrics[i].width != 0 && m_glyphMetrics[i].height != 0);
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

        // debugging
        if (!atlasBitmap->Write("TmpAtlas.bmp"))
        {
            HYP_FAIL("what");
        }

        // Create the atlas texture

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

    const auto index = std::distance(m_symbolList.Begin(), it);
    AssertThrow(index < m_glyphMetrics.Size(), "Index (%u) out of bounds of glyph metrics buffer (%u)", index, m_glyphMetrics.Size());

    return m_glyphMetrics[index];
}

void FontAtlas::WriteToBuffer(uint32 pixelSize, ByteBuffer& buffer) const
{
    Handle<Texture> atlas = m_atlasTextures.GetAtlasForPixelSize(pixelSize);

    if (!atlas.IsValid())
    {
        HYP_LOG(Font, Error, "Failed to get atlas for pixel size {}", pixelSize);

        return;
    }

    const uint32 bufferSize = atlas->GetTextureDesc().GetByteSize();
    buffer.SetSize(bufferSize);

    struct RENDER_COMMAND(FontAtlas_WriteToBuffer)
        : RenderCommand
    {
        Handle<Texture> atlas;
        ByteBuffer& outBuffer;
        uint32 bufferSize;

        GpuBufferRef buffer;

        RENDER_COMMAND(FontAtlas_WriteToBuffer)(Handle<Texture> atlas, ByteBuffer& outBuffer, uint32 bufferSize)
            : atlas(std::move(atlas)),
              outBuffer(outBuffer),
              bufferSize(bufferSize)
        {
        }

        virtual ~RENDER_COMMAND(FontAtlas_WriteToBuffer)() override
        {
            g_safeDeleter->SafeRelease(std::move(atlas));

            SafeRelease(std::move(buffer));
        }

        virtual RendererResult operator()() override
        {
            RendererResult result;

            buffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, bufferSize);
            HYPERION_ASSERT_RESULT(buffer->Create());

            if (!result)
            {
                return result;
            }

            SingleTimeCommands commands;

            AssertThrow(atlas);
            AssertThrow(atlas->IsReady());

            commands.Push([&](CmdList& cmd)
                {
                    // put src image in state for copying from
                    cmd.Add<InsertBarrier>(atlas->GetRenderResource().GetImage(), RS_COPY_SRC);

                    // put dst buffer in state for copying to
                    cmd.Add<InsertBarrier>(buffer, RS_COPY_DST);

                    cmd.Add<CopyImageToBuffer>(atlas->GetRenderResource().GetImage(), buffer);

                    cmd.Add<InsertBarrier>(buffer, RS_COPY_SRC);
                });

            HYPERION_PASS_ERRORS(commands.Execute(), result);

            buffer->Read(bufferSize, outBuffer.Data());

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(FontAtlas_WriteToBuffer, atlas, buffer, bufferSize);
    HYP_SYNC_RENDER();
}

Bitmap<1> FontAtlas::GenerateBitmap(uint32 pixelSize) const
{
    Handle<Texture> atlas = m_atlasTextures.GetAtlasForPixelSize(pixelSize);

    if (!atlas.IsValid())
    {
        HYP_LOG(Font, Error, "Failed to get atlas for pixel size {}", pixelSize);

        return {};
    }

    ByteBuffer byteBuffer;
    WriteToBuffer(pixelSize, byteBuffer);

    Bitmap<1> bitmap(atlas->GetExtent().x, atlas->GetExtent().y);
    bitmap.SetPixels(byteBuffer);
    bitmap.FlipVertical();

    return bitmap;
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
