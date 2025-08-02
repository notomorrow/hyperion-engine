/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/utilities/Result.hpp>

#include <core/memory/UniquePtr.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <rendering/Shared.hpp>

namespace hyperion {

class Texture;

using GlyphBitmap = Bitmap<4, ubyte>;

struct GlyphImageData
{
    Vec2i dimensions;
    ByteBuffer byteBuffer;

    HYP_API UniquePtr<GlyphBitmap> CreateBitmap() const;
};

class Glyph
{
public:
    struct Metrics
    {
        uint16 width = 0;
        uint16 height = 0;
        int16 bearingX = 0;
        int16 bearingY = 0;
        uint32 advance = 0;

        Vec2i imagePosition;
    };

    HYP_API Glyph(RC<FontFace> face, FontFace::GlyphIndex index, float scale);

    Glyph(const Glyph& other) = default;
    Glyph& operator=(const Glyph& other) = default;
    Glyph(Glyph&& other) noexcept = default;
    Glyph& operator=(Glyph&& other) noexcept = default;

    ~Glyph() = default;

    HYP_FORCE_INLINE const Metrics& GetMetrics() const
    {
        return m_metrics;
    }

    HYP_FORCE_INLINE const GlyphImageData& GetImageData() const
    {
        return m_glyphImageData;
    }

    HYP_API void LoadMetrics();
    HYP_API TResult<UniquePtr<GlyphBitmap>> Rasterize();

    HYP_API Vec2i GetMax();
    HYP_API Vec2i GetMin();

private:
    RC<FontFace> m_face;
    FontFace::GlyphIndex m_index;
    float m_scale;

    GlyphImageData m_glyphImageData;
    Metrics m_metrics {};
};

}; // namespace hyperion
