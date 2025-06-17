/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BITMAP_HPP
#define HYPERION_BITMAP_HPP

#include <core/io/ByteWriter.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>
#include <core/Util.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/MathUtil.hpp>
#include <core/math/Rect.hpp>

#include <util/img/WriteBitmap.hpp>

#include <Types.hpp>

namespace hyperion {

template <class ComponentType, uint32 NumComponents>
struct Pixel
{
    static constexpr uint32 numComponents = NumComponents;

    ComponentType components[NumComponents];

    Pixel() = default;

    Pixel(Color color)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(float(color.r) * 255.0f);

            if constexpr (numComponents >= 2)
            {
                components[1] = ubyte(float(color.g) * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = ubyte(float(color.b) * 255.0f);
            }

            if constexpr (numComponents >= 4)
            {
                components[3] = ubyte(float(color.a) * 255.0f);
            }
        }
        else
        {
            components[0] = color.r;

            if constexpr (numComponents >= 2)
            {
                components[1] = color.g;
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = color.b;
            }

            if constexpr (numComponents >= 4)
            {
                components[3] = color.a;
            }
        }
    }

    Pixel(Vec2f rg)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(rg.x * 255.0f);

            if constexpr (numComponents >= 2)
            {
                components[1] = ubyte(rg.y * 255.0f);
            }
        }
        else
        {
            components[0] = rg.x;

            if constexpr (numComponents >= 2)
            {
                components[1] = rg.y;
            }
        }
    }

    Pixel(Vec3f rgb)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(rgb.x * 255.0f);

            if constexpr (numComponents >= 2)
            {
                components[1] = ubyte(rgb.y * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = ubyte(rgb.z * 255.0f);
            }
        }
        else
        {
            components[0] = rgb.x;

            if constexpr (numComponents >= 2)
            {
                components[1] = rgb.y;
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = rgb.z;
            }
        }
    }

    Pixel(Vec4f rgba)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(rgba.x * 255.0f);

            if constexpr (numComponents >= 2)
            {
                components[1] = ubyte(rgba.y * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = ubyte(rgba.z * 255.0f);
            }

            if constexpr (numComponents >= 4)
            {
                components[3] = ubyte(rgba.w * 255.0f);
            }
        }
        else
        {
            components[0] = rgba.x;

            if constexpr (numComponents >= 2)
            {
                components[1] = rgba.y;
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = rgba.z;
            }

            if constexpr (numComponents >= 4)
            {
                components[3] = rgba.w;
            }
        }
    }

    Pixel(ComponentType r)
    {
        components[0] = r;
    }

    Pixel(ComponentType r, ComponentType g)
    {
        components[0] = r;

        if constexpr (numComponents >= 2)
        {
            components[1] = g;
        }
    }

    Pixel(ComponentType r, ComponentType g, ComponentType b)
    {
        components[0] = r;

        if constexpr (numComponents >= 2)
        {
            components[1] = g;
        }

        if constexpr (numComponents >= 3)
        {
            components[2] = b;
        }
    }

    Pixel(ComponentType r, ComponentType g, ComponentType b, ComponentType a)
    {
        components[0] = r;

        if constexpr (numComponents >= 2)
        {
            components[1] = g;
        }

        if constexpr (numComponents >= 3)
        {
            components[2] = b;
        }

        if constexpr (numComponents >= 4)
        {
            components[3] = a;
        }
    }

    Pixel(const Pixel& other) = default;
    Pixel& operator=(const Pixel& other) = default;
    Pixel(Pixel&& other) noexcept = default;
    Pixel& operator=(Pixel&& other) noexcept = default;
    ~Pixel() = default;

    float GetComponent(uint32 index) const
    {
        if (index >= numComponents)
        {
            return 0.0f;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            return float(components[index]) / 255.0f;
        }
        else
        {
            return components[index];
        }
    }

    void SetComponent(uint32 index, float value)
    {
        if (index >= numComponents)
        {
            return;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[index] = ubyte(value * 255.0f);
        }
        else
        {
            components[index] = value;
        }
    }

    void SetR(float r)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(r * 255.0f);
        }
        else
        {
            components[0] = r;
        }
    }

    void SetRG(float r, float g)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                components[1] = ubyte(g * 255.0f);
            }
        }
        else
        {
            components[0] = r;

            if constexpr (numComponents >= 2)
            {
                components[1] = g;
            }
        }
    }

    void SetRGB(float r, float g, float b)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                components[1] = ubyte(g * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = ubyte(b * 255.0f);
            }
        }
        else
        {
            components[0] = r;

            if constexpr (numComponents >= 2)
            {
                components[1] = g;
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = b;
            }
        }
    }

    void SetRGBA(float r, float g, float b, float a)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            components[0] = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                components[1] = ubyte(g * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = ubyte(b * 255.0f);
            }

            if constexpr (numComponents >= 4)
            {
                components[3] = ubyte(a * 255.0f);
            }
        }
        else
        {
            components[0] = r;

            if constexpr (numComponents >= 2)
            {
                components[1] = g;
            }

            if constexpr (numComponents >= 3)
            {
                components[2] = b;
            }

            if constexpr (numComponents >= 4)
            {
                components[3] = a;
            }
        }
    }

    Vec3f GetRGB() const
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            return Vec3f(
                float(components[0]) / 255.0f,
                float(components[1]) / 255.0f,
                float(components[2]) / 255.0f);
        }
        else
        {
            return Vec3f(
                components[0],
                components[1],
                components[2]);
        }
    }

    void SetRGB(const Vec3f& rgb)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            for (uint32 i = 0; i < MathUtil::Min(NumComponents, 3); i++)
            {
                components[i] = ubyte(rgb[i] * 255.0f);
            }
        }
        else
        {
            for (uint32 i = 0; i < MathUtil::Min(NumComponents, 3); i++)
            {
                components[i] = rgb[i];
            }
        }
    }

    HYP_FORCE_INLINE
    operator Vec3f() const
    {
        return GetRGB();
    }

    Vec4f GetRGBA() const
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            if constexpr (NumComponents < 4)
            {
                return Vec4f(
                    float(components[0]) / 255.0f,
                    float(components[1]) / 255.0f,
                    float(components[2]) / 255.0f,
                    1.0f);
            }
            else
            {
                return Vec4f(
                    float(components[0]) / 255.0f,
                    float(components[1]) / 255.0f,
                    float(components[2]) / 255.0f,
                    float(components[3]) / 255.0f);
            }
        }
        else
        {
            if constexpr (NumComponents < 4)
            {
                return Vec4f(
                    components[0],
                    components[1],
                    components[2],
                    1.0f);
            }
            else
            {
                return Vec4f(
                    components[0],
                    components[1],
                    components[2],
                    components[3]);
            }
        }
    }

    void SetRGBA(const Vec4f& rgba)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            for (uint32 i = 0; i < MathUtil::Min(NumComponents, 4); i++)
            {
                components[i] = ubyte(rgba[i] * 255.0f);
            }
        }
        else
        {
            for (uint32 i = 0; i < MathUtil::Min(NumComponents, 4); i++)
            {
                components[i] = rgba[i];
            }
        }
    }

    HYP_FORCE_INLINE
    operator Vec4f() const
    {
        return GetRGBA();
    }
};

template <uint32 NumComponents, class PixelComponentType = ubyte>
class Bitmap
{
public:
    using PixelType = Pixel<PixelComponentType, NumComponents>;

    Bitmap()
        : m_width(0),
          m_height(0)
    {
    }

    Bitmap(const Array<float>& floats, uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);

        const uint32 numComponents = PixelType::numComponents;

        for (uint32 i = 0, j = 0; i < floats.Size() && j < m_pixels.Size(); i += numComponents, j++)
        {
            for (uint32 k = 0; k < numComponents; k++)
            {
                m_pixels[j].SetComponent(k, floats[i + k]);
            }
        }
    }

    Bitmap(ConstByteView bytes, uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);

        AssertDebug(bytes.Size() == GetByteSize(), "Byte view size does not match bitmap size! (%u != %u)", bytes.Size(), GetByteSize());

        Memory::MemCpy(m_pixels.Data(), bytes.Data(), MathUtil::Min(m_pixels.ByteSize(), bytes.Size()));
    }

    Bitmap(uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);
    }

    Bitmap(const Bitmap& other)
        : m_width(other.m_width),
          m_height(other.m_height),
          m_pixels(other.m_pixels)
    {
    }

    Bitmap& operator=(const Bitmap& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = other.m_pixels;

        return *this;
    }

    Bitmap(Bitmap&& other) noexcept
        : m_width(other.m_width),
          m_height(other.m_height),
          m_pixels(std::move(other.m_pixels))
    {
    }

    Bitmap& operator=(Bitmap&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = std::move(other.m_pixels);

        return *this;
    }

    ~Bitmap() = default;

    HYP_FORCE_INLINE uint32 GetWidth() const
    {
        return m_width;
    }

    HYP_FORCE_INLINE uint32 GetHeight() const
    {
        return m_height;
    }

    HYP_FORCE_INLINE SizeType GetByteSize() const
    {
        return SizeType(m_width)
            * SizeType(m_height)
            * SizeType(PixelType::numComponents)
            * sizeof(PixelComponentType);
    }

    HYP_FORCE_INLINE PixelType& GetPixelAtIndex(uint32 index)
    {
        return m_pixels[index];
    }

    HYP_FORCE_INLINE const PixelType& GetPixelAtIndex(uint32 index) const
    {
        return m_pixels[index];
    }

    HYP_FORCE_INLINE PixelType& GetPixel(uint32 x, uint32 y)
    {
        const uint32 index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    HYP_FORCE_INLINE const PixelType& GetPixel(uint32 x, uint32 y) const
    {
        const uint32 index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    HYP_FORCE_INLINE void SetPixel(uint32 x, uint32 y, PixelType pixel)
    {
        const uint32 index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        m_pixels[index] = pixel;
    }

    void SetPixels(const ByteBuffer& byteBuffer)
    {
        AssertThrowMsg(byteBuffer.Size() == GetByteSize(), "Byte buffer size does not match bitmap size! (%u != %u)", byteBuffer.Size(), GetByteSize());

        const uint32 numComponents = PixelType::numComponents;

        m_pixels.Resize(byteBuffer.Size() / numComponents);

        for (SizeType i = 0, j = 0; i < byteBuffer.Size() && j < m_pixels.Size(); i += numComponents, j++)
        {
            for (uint32 k = 0; k < numComponents; k++)
            {
                m_pixels[j].SetComponent(k, float(byteBuffer.Data()[i + k] / 255.0f));
            }
        }
    }

    ByteBuffer ToByteBuffer() const
    {
        ByteBuffer byteBuffer;
        byteBuffer.SetSize(GetByteSize());

        for (SizeType i = 0, j = 0; i < byteBuffer.Size() && j < m_pixels.Size(); i += PixelType::numComponents * sizeof(PixelComponentType), j++)
        {
            for (uint32 k = 0; k < PixelType::numComponents; k++)
            {
                Memory::MemCpy(&byteBuffer.Data()[i + k * sizeof(PixelComponentType)], &m_pixels[j].components[k], sizeof(PixelComponentType));
            }
        }

        return byteBuffer;
    }

    ByteBuffer GetUnpackedBytes(uint32 bytesPerPixel) const
    {
        ByteBuffer byteBuffer;
        byteBuffer.SetSize(m_pixels.Size() * bytesPerPixel);

        ubyte* bytes = byteBuffer.Data();

        if (bytesPerPixel == 1)
        {
            for (uint32 i = 0; i < m_pixels.Size(); i++)
            {
                Vec4f pixel;

                for (uint32 j = 0; j < PixelType::numComponents; j++)
                {
                    pixel[j] = m_pixels[i].GetComponent(j);
                }

                const Color color { pixel };

                for (uint32 j = 0; j < MathUtil::Min(PixelType::numComponents, bytesPerPixel); j++)
                {
                    bytes[i * bytesPerPixel + j] = color.bytes[j];
                }
            }
        }
        else
        {
            for (uint32 i = 0; i < m_pixels.Size(); i++)
            {
                for (uint32 j = 0; j < MathUtil::Min(PixelType::numComponents, bytesPerPixel); j++)
                {
                    bytes[i * bytesPerPixel + j] = ubyte(m_pixels[i].GetComponent(j) * 255.0f);
                }
            }
        }

        return byteBuffer;
    }

    Array<float> GetUnpackedFloats() const
    {
        Array<float> floats;
        floats.Resize(m_pixels.Size() * PixelType::numComponents);

        for (uint32 i = 0; i < m_pixels.Size(); i++)
        {
            for (uint32 j = 0; j < PixelType::numComponents; j++)
            {
                floats[i * PixelType::numComponents + j] = m_pixels[i].GetComponent(j);
            }
        }

        return floats;
    }

    bool Write(const String& filepath) const
    {
        // WriteBitmap uses 3 bytes per pixel
        ByteBuffer unpackedBytes = GetUnpackedBytes(3);

        // BMP stores in BGR format, so swap R and B
        for (uint32 i = 0; i < unpackedBytes.Size(); i += 3)
        {
            Swap(unpackedBytes[i], unpackedBytes[i + 2]);
        }

        return WriteBitmap::Write(filepath.Data(), m_width, m_height, unpackedBytes.Data());
    }

    void FlipVertical()
    {
        for (uint32 x = 0; x < m_width; x++)
        {
            for (uint32 y = 0; y < m_height / 2; y++)
            {
                auto temp = GetPixel(x, m_height - y - 1u);
                GetPixel(x, m_height - y - 1u) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

    void FlipHorizontal()
    {
        for (uint32 x = 0; x < m_width / 2; x++)
        {
            for (uint32 y = 0; y < m_height; y++)
            {
                auto temp = GetPixel(m_width - x - 1u, y);
                GetPixel(m_width - x - 1u, y) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

    void FillRectangle(
        Vec2i p0,
        Vec2i p1,
        PixelType color)
    {
        for (int y = p0.y; y <= p1.y; y++)
        {
            for (int x = p0.x; x <= p1.x; x++)
            {
                SetPixel(x, y, color);
            }
        }
    }

    // https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
    void FillTriangle(
        Vec2i t0,
        Vec2i t1,
        Vec2i t2,
        PixelType color)
    {
        if (t0.y > t1.y)
        {
            std::swap(t0, t1);
        }

        if (t0.y > t2.y)
        {
            std::swap(t0, t2);
        }

        if (t1.y > t2.y)
        {
            std::swap(t1, t2);
        }

        int totalHeight = t2.y - t0.y;

        if (totalHeight == 0)
        {
            return;
        }

        for (int y = t0.y; y <= t1.y; y++)
        {
            int segmentHeight = t1.y - t0.y + 1;
            float alpha = (float)(y - t0.y) / totalHeight;
            float beta = (float)(y - t0.y) / segmentHeight;

            Vec2i a = t0 + (t2 - t0) * alpha;
            Vec2i b = t0 + (t1 - t0) * beta;

            if (a.x > b.x)
            {
                std::swap(a, b);
            }

            for (int j = a.x; j <= b.x; j++)
            {
                SetPixel(j, y, color);
            }
        }

        for (int y = t1.y; y <= t2.y; y++)
        {
            int segmentHeight = t2.y - t1.y + 1;
            float alpha = (float)(y - t0.y) / totalHeight;
            float beta = (float)(y - t1.y) / segmentHeight;

            Vec2i a = t0 + (t2 - t0) * alpha;
            Vec2i b = t1 + (t2 - t1) * beta;

            if (a.x > b.x)
            {
                std::swap(a, b);
            }

            for (int j = a.x; j <= b.x; j++)
            {
                SetPixel(j, y, color);
            }
        }
    }

    void DrawLine(uint32 x0, uint32 y0, uint32 x1, uint32 y1, PixelType color)
    {
        const int dx = MathUtil::Abs(int(x1) - int(x0));
        const int dy = MathUtil::Abs(int(y1) - int(y0));

        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;

        int err = dx - dy;

        while (true)
        {
            GetPixel(x0, y0) = color;

            if (x0 == x1 && y0 == y1)
            {
                break;
            }

            int e2 = 2 * err;

            if (e2 > -dy)
            {
                err -= dy;
                x0 += sx;
            }

            if (e2 < dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

    /*! \brief Blit another bitmap onto this bitmap at the specified offset.
     *
     * \param other The bitmap to blit.
     * \param offset The offset to blit the other bitmap at.
     * \param extent The dimensions of the area to blit.
     */
    template <uint32 OtherNumComponents, class OtherPixelComponentType>
    void Blit(const Bitmap<OtherNumComponents, OtherPixelComponentType>& other, Rect<uint32> srcRect, Rect<uint32> dstRect)
    {
        const int dstStartX = dstRect.x0;
        const int dstEndX = dstRect.x1;

        const int dstStartY = dstRect.y0;
        const int dstEndY = dstRect.y1;

        const int dstWidth = MathUtil::Abs(dstEndX - dstStartX);
        const int dstHeight = MathUtil::Abs(dstEndY - dstStartY);

        const int dstStepX = (dstEndX > dstStartX) ? 1 : -1;
        const int dstStepY = (dstEndY > dstStartY) ? 1 : -1;

        const float srcStartX = float(srcRect.x0);
        const float srcEndX = float(srcRect.x1);
        const float srcStartY = float(srcRect.y0);
        const float srcEndY = float(srcRect.y1);
        const float srcWidth = srcEndX - srcStartX;
        const float srcHeight = srcEndY - srcStartY;

        for (int j = 0; j < dstHeight; j++)
        {
            const float srcY = srcStartY + ((float(j) + 0.5f) / float(dstHeight)) * srcHeight;

            int sy0 = int(MathUtil::Floor(srcY));
            int sy1 = sy0 + 1;

            const float ty = srcY - float(sy0);

            sy0 = MathUtil::Max(sy0, 0);
            sy1 = MathUtil::Max(sy1, 0);

            for (int i = 0; i < dstWidth; i++)
            {
                const float srcX = srcStartX + ((float(i) + 0.5f) / float(dstWidth)) * srcWidth;

                int sx0 = int(MathUtil::Floor(srcX));
                int sx1 = sx0 + 1;

                const float tx = srcX - float(sx0);

                sx0 = MathUtil::Max(sx0, 0);
                sx1 = MathUtil::Max(sx1, 0);

                // Fetch the four neighboring source pixels and convert them to normalized RGBA (0.0 - 1.0)
                const Vec4f c00 = sx0 < other.GetWidth() && sy0 < other.GetHeight() ? other.GetPixel(sx0, sy0).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c10 = sx1 < other.GetWidth() && sy0 < other.GetHeight() ? other.GetPixel(sx1, sy0).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c01 = sx0 < other.GetWidth() && sy1 < other.GetHeight() ? other.GetPixel(sx0, sy1).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c11 = sx1 < other.GetWidth() && sy1 < other.GetHeight() ? other.GetPixel(sx1, sy1).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

                // Perform bilinear interpolation.
                const Vec4f c0 = MathUtil::Lerp(c00, c10, tx);
                const Vec4f c1 = MathUtil::Lerp(c01, c11, tx);

                Vec4f resultColor = MathUtil::Lerp(c0, c1, ty);

                // Write the pixel to the destination bitmap.
                SetPixel(uint32(dstStartX + i * dstStepX), uint32(dstStartY + j * dstStepY), PixelType(resultColor));
            }
        }
    }

private:
    uint32 m_width;
    uint32 m_height;
    Array<PixelType> m_pixels;
};

} // namespace hyperion

#endif
