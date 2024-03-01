#ifndef HYPERION_V2_BITMAP_HPP
#define HYPERION_V2_BITMAP_HPP

#include <asset/ByteWriter.hpp>
#include <core/Containers.hpp>
#include <core/lib/String.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/MathUtil.hpp>
#include <util/ByteUtil.hpp>
#include <util/img/WriteBitmap.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

template <uint NumComponents>
struct Pixel
{
    static constexpr uint byte_size = NumComponents > 1 ? NumComponents : 1;

    ubyte bytes[byte_size];

    Pixel() = default;

    Pixel(Vec2f rg)
    {
        bytes[0] = static_cast<ubyte>(rg.x * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<ubyte>(rg.y * 255.0f);
        }
    }

    Pixel(Vec3f rgb)
    {
        bytes[0] = static_cast<ubyte>(rgb.x * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<ubyte>(rgb.y * 255.0f);
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = static_cast<ubyte>(rgb.z * 255.0f);
        }
    }

    Pixel(Vec4f rgba)
    {
        bytes[0] = static_cast<ubyte>(rgba.x * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<ubyte>(rgba.y * 255.0f);
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = static_cast<ubyte>(rgba.z * 255.0f);
        }

        if constexpr (byte_size >= 4) {
            bytes[3] = static_cast<ubyte>(rgba.w * 255.0f);
        }
    }

    Pixel(ubyte r)
    {
        bytes[0] = r;
    }

    Pixel(ubyte r, ubyte g)
    {
        bytes[0] = r;

        if constexpr (byte_size >= 2) {
            bytes[1] = g;
        }
    }

    Pixel(ubyte r, ubyte g, ubyte b)
    {
        bytes[0] = r;

        if constexpr (byte_size >= 2) {
            bytes[1] = g;
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = b;
        }
    }

    Pixel(ubyte r, ubyte g, ubyte b, ubyte a)
    {
        bytes[0] = r;

        if constexpr (byte_size >= 2) {
            bytes[1] = g;
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = b;
        }

        if constexpr (byte_size >= 4) {
            bytes[3] = a;
        }
    }

    void SetR(float r)
    {
        bytes[0] = static_cast<ubyte>(r * 255.0f);
    }

    void SetRG(float r, float g)
    {
        bytes[0] = static_cast<ubyte>(r * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<ubyte>(g * 255.0f);
        }
    }

    void SetRGB(float r, float g, float b)
    {
        bytes[0] = static_cast<ubyte>(r * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<ubyte>(g * 255.0f);
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = static_cast<ubyte>(b * 255.0f);
        }
    }

    void SetRGBA(float r, float g, float b, float a)
    {
        bytes[0] = static_cast<ubyte>(r * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<ubyte>(g * 255.0f);
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = static_cast<ubyte>(b * 255.0f);
        }

        if constexpr (byte_size >= 4) {
            bytes[3] = static_cast<ubyte>(a * 255.0f);
        }
    }

    Pixel(const Pixel &other)                   = default;
    Pixel &operator=(const Pixel &other)        = default;
    Pixel(Pixel &&other) noexcept               = default;
    Pixel &operator=(Pixel &&other) noexcept    = default;
    ~Pixel()                                    = default;

    Vector3 GetRGB() const
    {
        return Vector3(
            static_cast<float>(bytes[2]) / 255.0f,
            static_cast<float>(bytes[1]) / 255.0f,
            static_cast<float>(bytes[0]) / 255.0f
        );
    }

    void SetRGB(const Vector3 &rgb)
    {
        for (uint i = 0; i < MathUtil::Min(byte_size, 3); i++) {
            bytes[byte_size - i - 1] = static_cast<ubyte>(rgb[i] * 255.0f);
        }
    }

    Vector4 GetRGBA() const
    {
        if constexpr (byte_size < 4) {
            return Vector4(
                static_cast<float>(bytes[2]) / 255.0f,
                static_cast<float>(bytes[1]) / 255.0f,
                static_cast<float>(bytes[0]) / 255.0f,
                1.0f
            );
        } else {
            return Vector4(
                static_cast<float>(bytes[3]) / 255.0f,
                static_cast<float>(bytes[2]) / 255.0f,
                static_cast<float>(bytes[1]) / 255.0f,
                static_cast<float>(bytes[0]) / 255.0f
            );
        }
    }

    void SetRGBA(const Vector4 &rgba)
    {
        for (uint i = 0; i < MathUtil::Min(byte_size, 4); i++) {
            bytes[byte_size - i - 1] = static_cast<ubyte>(rgba[i] * 255.0f);
        }
    }
};

template <uint NumComponents>
class Bitmap
{
public:
    using PixelType = Pixel<NumComponents>;

    Bitmap()
        : m_width(0),
          m_height(0)
    {
    }

    Bitmap(const Array<float> &floats, uint width, uint height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);

        const uint bpp = PixelType::byte_size;

        for (uint i = 0, j = 0; i < floats.Size() && j < m_pixels.Size(); i += bpp, j++) {
            for (uint k = 0; k < bpp; k++) {
                m_pixels[j].bytes[k] = static_cast<ubyte>(floats[i + k] * 255.0f);
            }
        }
    }

    Bitmap(uint width, uint height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);
    }

    Bitmap(const Bitmap &other)
        : m_width(other.m_width),
          m_height(other.m_height),
          m_pixels(other.m_pixels)
    {
    }

    Bitmap &operator=(const Bitmap &other)
    {
        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = other.m_pixels;

        return *this;
    }

    Bitmap(Bitmap &&other) noexcept
        : m_width(other.m_width),
          m_height(other.m_height),
          m_pixels(std::move(other.m_pixels))
    {
    }

    Bitmap &operator=(Bitmap &&other) noexcept
    {
        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = std::move(other.m_pixels);

        return *this;
    }

    ~Bitmap() = default;

    uint GetWidth() const
        { return m_width; }

    uint GetHeight() const
        { return m_height; }

    SizeType GetByteSize() const
    {
        return static_cast<SizeType>(m_width)
            * static_cast<SizeType>(m_height)
            * static_cast<SizeType>(NumComponents);
    }

    PixelType &GetPixelAtIndex(uint index)
        { return m_pixels[index]; }

    const PixelType &GetPixelAtIndex(uint index) const
        { return m_pixels[index]; }

    PixelType &GetPixel(uint x, uint y)
    {
        const uint index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    const PixelType &GetPixel(uint x, uint y) const
        { return const_cast<const Bitmap *>(this)->GetPixel(x, y); }

    void SetPixel(uint x, uint y, PixelType pixel)
    {
        const uint index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        m_pixels[index] = pixel;
    }

    ByteBuffer ToByteBuffer() const
    {
        ByteBuffer byte_buffer;
        byte_buffer.SetSize(m_pixels.Size() * PixelType::byte_size);

        for (SizeType i = 0, j = 0; i < byte_buffer.Size() && j < m_pixels.Size(); i += PixelType::byte_size, j++) {
            for (uint k = 0; k < PixelType::byte_size; k++) {
                byte_buffer.Data()[i + k] = m_pixels[j].bytes[k];
            }
        }

        return byte_buffer;
    }

    Array<ubyte> GetUnpackedBytes() const
    {
        Array<ubyte> bytes;
        bytes.Resize(m_pixels.Size() * PixelType::byte_size);

        for (uint i = 0; i < m_pixels.Size(); i++) {
            for (uint j = 0; j < PixelType::byte_size; j++) {
                bytes[i * PixelType::byte_size + j] = m_pixels[i].bytes[j];
            }
        }

        return bytes;
    }

    Array<ubyte> GetUnpackedBytes(uint bytes_per_pixel) const
    {
        Array<ubyte> bytes;
        bytes.Resize(m_pixels.Size() * bytes_per_pixel);

        for (uint i = 0; i < m_pixels.Size(); i++) {
            for (uint j = 0; j < MathUtil::Min(PixelType::byte_size, bytes_per_pixel); j++) {
                bytes[i * bytes_per_pixel + j] = m_pixels[i].bytes[j];
            }
        }

        return bytes;
    }

    Array<float> GetUnpackedFloats() const
    {
        Array<float> floats;
        floats.Resize(m_pixels.Size() * PixelType::byte_size);

        for (uint i = 0; i < m_pixels.Size(); i++) {
           for (uint j = 0; j < PixelType::byte_size; j++) {
                floats[i * PixelType::byte_size + j] = static_cast<float>(m_pixels[i].bytes[j]) / 255.0f;
           }
        }

        return floats;
    }

    void Write(const String &filepath) const
    {
        Array<ubyte> unpacked_bytes = GetUnpackedBytes(3 /* WriteBitmap uses 3 bytes per pixel */);

        // BMP stores in BGR format, so swap R and B
        for (uint i = 0; i < unpacked_bytes.Size(); i += 3) {
            std::swap(unpacked_bytes[i], unpacked_bytes[i + 2]);
        }

        WriteBitmap::Write(filepath.Data(), m_width, m_height, unpacked_bytes.Data());
    }

    void FlipVertical()
    {
        for (uint x = 0; x < m_width; x++) {
            for (uint y = 0; y < m_height / 2; y++) {
                auto temp = GetPixel(x, m_height - y - 1u);
                GetPixel(x, m_height - y - 1u) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

    void FlipHorizontal()
    {
        for (uint x = 0; x < m_width / 2; x++) {
            for (uint y = 0; y < m_height; y++) {
                auto temp = GetPixel(m_width - x - 1u, y);
                GetPixel(m_width - x - 1u, y) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

    // https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
    void FillTriangle(
        Vec2i t0,
        Vec2i t1,
        Vec2i t2,
        PixelType color
    )
    {
        if (t0.y > t1.y) {
            std::swap(t0, t1);
        }

        if (t0.y > t2.y) {
            std::swap(t0, t2);
        }

        if (t1.y > t2.y) {
            std::swap(t1, t2);
        }

        int total_height = t2.y-t0.y;

        if (total_height == 0) {
            return;
        }

        for (int y = t0.y; y <= t1.y; y++) { 
            int segment_height = t1.y - t0.y + 1; 
            float alpha = (float)(y - t0.y) / total_height;
            float beta = (float)(y - t0.y) / segment_height;

            Vec2i A = t0 + (t2 - t0) * alpha;
            Vec2i B = t0 + (t1 - t0) * beta;

            if (A.x > B.x) {
                std::swap(A, B);
            }

            for (int j = A.x; j <= B.x; j++) {
                SetPixel(j, y, color);
            }
        }

        for (int y = t1.y; y <= t2.y; y++) {
            int segment_height = t2.y - t1.y + 1;
            float alpha = (float)(y - t0.y) / total_height;
            float beta = (float)(y - t1.y) / segment_height;

            Vec2i A = t0 + (t2 - t0) * alpha;
            Vec2i B = t1 + (t2 - t1) * beta;

            if (A.x > B.x) {
                std::swap(A, B);
            }

            for (int j = A.x; j <= B.x; j++) {
                SetPixel(j, y, color);
            }
        }
    }

    void DrawLine(uint x0, uint y0, uint x1, uint y1, PixelType color)
    {
        const int dx = MathUtil::Abs(int(x1) - int(x0));
        const int dy = MathUtil::Abs(int(y1) - int(y0));

        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;

        int err = dx - dy;

        while (true) {
            GetPixel(x0, y0) = color;

            if (x0 == x1 && y0 == y1) {
                break;
            }

            int e2 = 2 * err;

            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }

            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

private:
    uint                m_width;
    uint                m_height;
    Array<PixelType>    m_pixels;

};

} // namespace hyperion::v2

#endif
