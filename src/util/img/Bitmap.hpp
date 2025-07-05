/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BITMAP_HPP
#define HYPERION_BITMAP_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/utilities/Span.hpp>

#include <core/Util.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/MathUtil.hpp>
#include <core/math/Rect.hpp>

#include <util/img/WriteBitmap.hpp>

#include <Types.hpp>

namespace hyperion {

class ByteWriter;

template <class ComponentType, uint32 NumComponents>
struct ConstPixelReference;

template <class ComponentType, uint32 NumComponents>
struct PixelReference
{
    static constexpr uint32 numComponents = NumComponents;

    ubyte* byteOffset;

    PixelReference() = default;

    PixelReference(ubyte* byteOffset)
        : byteOffset(byteOffset)
    {
    }

    PixelReference(const PixelReference& other) = default;
    PixelReference& operator=(const PixelReference& other) = default;
    PixelReference(PixelReference&& other) noexcept = default;
    PixelReference& operator=(PixelReference&& other) noexcept = default;
    ~PixelReference() = default;

    float GetComponent(uint32 index) const
    {
        if (index >= numComponents || !byteOffset)
        {
            return 0.0f;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            return float(*(byteOffset + index)) / 255.0f;
        }
        else
        {
            return float(*reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index)));
        }
    }

    void SetComponent(uint32 index, float value)
    {
        if (index >= numComponents || !byteOffset)
        {
            return;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *(byteOffset + index) = ubyte(value * 255.0f);
        }
        else
        {
            *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index)) = ComponentType(value);
        }
    }

    float GetR() const
    {
        return GetComponent(0);
    }

    void SetR(float r)
    {
        SetComponent(0, r);
    }

    float GetG() const
    {
        return GetComponent(1);
    }

    void SetG(float g)
    {
        SetComponent(1, g);
    }

    float GetB() const
    {
        return GetComponent(2);
    }

    void SetB(float b)
    {
        SetComponent(2, b);
    }

    float GetA() const
    {
        return GetComponent(3);
    }

    void SetA(float a)
    {
        SetComponent(3, a);
    }

    Vec2f GetRG() const
    {
        Vec2f rg = Vec2f(0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rg;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rg.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rg.y = float(*(byteOffset + 1)) / 255.0f;
            }
        }
        else
        {
            rg.x = *reinterpret_cast<ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rg.y = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }
        }

        return rg;
    }

    void SetRG(const Vec2f& rg)
    {
        SetRG(rg.x, rg.y);
    }

    void SetRG(float r, float g)
    {
        if (HYP_UNLIKELY(!byteOffset))
        {
            return;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *byteOffset = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                *(byteOffset + 1) = ubyte(g * 255.0f);
            }
        }
        else
        {
            *reinterpret_cast<ComponentType*>(byteOffset) = ComponentType(r);

            if constexpr (numComponents >= 2)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType)) = ComponentType(g);
            }
        }
    }

    Vec3f GetRGB() const
    {
        Vec3f rgb = Vec3f(0.0f, 0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgb;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgb.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgb.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = float(*(byteOffset + 2)) / 255.0f;
            }
        }
        else
        {
            rgb.x = *reinterpret_cast<ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgb.y = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }
        }

        return rgb;
    }

    void SetRGB(const Vec3f& rgb)
    {
        SetRGB(rgb.x, rgb.y, rgb.z);
    }

    void SetRGB(float r, float g, float b)
    {
        if (HYP_UNLIKELY(!byteOffset))
        {
            return;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *byteOffset = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                *(byteOffset + 1) = ubyte(g * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                *(byteOffset + 2) = ubyte(b * 255.0f);
            }
        }
        else
        {
            *reinterpret_cast<ComponentType*>(byteOffset) = ComponentType(r);

            if constexpr (numComponents >= 2)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType)) = ComponentType(g);
            }

            if constexpr (numComponents >= 3)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2) = ComponentType(b);
            }
        }
    }

    Vec4f GetRGBA() const
    {
        Vec4f rgba = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgba;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgba.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgba.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = float(*(byteOffset + 2)) / 255.0f;
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = float(*(byteOffset + 3)) / 255.0f;
            }
        }
        else
        {
            rgba.x = *reinterpret_cast<ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgba.y = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 3);
            }
        }

        return rgba;
    }

    void SetRGBA(const Vec4f& rgba)
    {
        SetRGBA(rgba.x, rgba.y, rgba.z, rgba.w);
    }

    void SetRGBA(float r, float g, float b, float a)
    {
        if (HYP_UNLIKELY(!byteOffset))
        {
            return;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *byteOffset = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                *(byteOffset + 1) = ubyte(g * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                *(byteOffset + 2) = ubyte(b * 255.0f);
            }

            if constexpr (numComponents >= 4)
            {
                *(byteOffset + 3) = ubyte(a * 255.0f);
            }
        }
        else
        {
            *reinterpret_cast<ComponentType*>(byteOffset) = ComponentType(r);

            if constexpr (numComponents >= 2)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType)) = ComponentType(g);
            }

            if constexpr (numComponents >= 3)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2) = ComponentType(b);
            }

            if constexpr (numComponents >= 4)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 3) = ComponentType(a);
            }
        }
    }

    void SetScalar(float scalar)
    {
        SetRGBA(Vec4f(scalar));
    }

    /*HYP_FORCE_INLINE PixelReference& operator=(float scalar)
    {
        SetRGBA(Vec4f(scalar));
        return *this;
    }

    HYP_FORCE_INLINE PixelReference& operator=(const Vec2f& rg)
    {
        SetRG(rg);
        return *this;
    }

    HYP_FORCE_INLINE PixelReference& operator=(const Vec3f& rgb)
    {
        SetRGB(rgb);
        return *this;
    }

    HYP_FORCE_INLINE PixelReference& operator=(const Vec4f& rgba)
    {
        SetRGBA(rgba);
        return *this;
    }

    HYP_FORCE_INLINE explicit operator float() const
    {
        return GetR();
    }

    HYP_FORCE_INLINE operator Vec2f() const
    {
        return GetRG();
    }

    HYP_FORCE_INLINE operator Vec3f() const
    {
        return GetRGB();
    }

    HYP_FORCE_INLINE operator Vec4f() const
    {
        return GetRGBA();
    }*/
};

template <class ComponentType, uint32 NumComponents>
struct ConstPixelReference
{
    static constexpr uint32 numComponents = NumComponents;

    const ubyte* byteOffset;

    ConstPixelReference()
        : byteOffset(nullptr)
    {
    }

    ConstPixelReference(const ubyte* byteOffset)
        : byteOffset(byteOffset)
    {
    }

    ConstPixelReference(const ConstPixelReference<ComponentType, NumComponents>& other) = default;
    ConstPixelReference& operator=(const ConstPixelReference<ComponentType, NumComponents>& other) = delete;

    ConstPixelReference(const PixelReference<ComponentType, NumComponents>& other)
        : byteOffset(other.byteOffset)
    {
    }

    ConstPixelReference& operator=(const PixelReference<ComponentType, NumComponents>& other)
    {
        byteOffset = other.byteOffset;
        return *this;
    }

    ConstPixelReference(ConstPixelReference<ComponentType, NumComponents>&& other) noexcept
        : byteOffset(other.byteOffset)
    {
        other.byteOffset = nullptr;
    }

    ConstPixelReference& operator=(ConstPixelReference<ComponentType, NumComponents>&& other)
    {
        byteOffset = other.byteOffset;
        other.byteOffset = nullptr;
        return *this;
    }

    float GetComponent(uint32 index) const
    {
        if (index >= numComponents || !byteOffset)
        {
            return 0.0f;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            return float(*(byteOffset + index)) / 255.0f;
        }
        else
        {
            return float(*reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index)));
        }
    }

    float GetR() const
    {
        return GetComponent(0);
    }

    float GetG() const
    {
        return GetComponent(1);
    }

    float GetB() const
    {
        return GetComponent(2);
    }

    float GetA() const
    {
        return GetComponent(3);
    }

    Vec2f GetRG() const
    {
        Vec2f rg = Vec2f(0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rg;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rg.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rg.y = float(*(byteOffset + 1)) / 255.0f;
            }
        }
        else
        {
            rg.x = *reinterpret_cast<const ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rg.y = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }
        }

        return rg;
    }

    Vec3f GetRGB() const
    {
        Vec3f rgb = Vec3f(0.0f, 0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgb;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgb.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgb.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = float(*(byteOffset + 2)) / 255.0f;
            }
        }
        else
        {
            rgb.x = *reinterpret_cast<const ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgb.y = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }
        }

        return rgb;
    }

    Vec4f GetRGBA() const
    {
        Vec4f rgba = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgba;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgba.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgba.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = float(*(byteOffset + 2)) / 255.0f;
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = float(*(byteOffset + 3)) / 255.0f;
            }
        }
        else
        {
            rgba.x = *reinterpret_cast<const ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgba.y = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 3);
            }
        }

        return rgba;
    }

    /*

    HYP_FORCE_INLINE explicit operator float() const
    {
        return GetR();
    }

    HYP_FORCE_INLINE operator Vec2f() const
    {
        return GetRG();
    }

    HYP_FORCE_INLINE operator Vec3f() const
    {
        return GetRGB();
    }

    HYP_FORCE_INLINE operator Vec4f() const
    {
        return GetRGBA();
    }*/
};

template <uint32 NumComponents, class PixelComponentType = ubyte>
class Bitmap
{
public:
    Bitmap()
        : m_width(0),
          m_height(0)
    {
    }

    Bitmap(Span<const float> float_data, uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_buffer.SetSize(GetByteSize());

        const SizeType numPixels = m_width * m_height;

        for (SizeType i = 0, j = 0; i < float_data.Size() && j < numPixels; i += NumComponents, j++)
        {
            for (uint32 k = 0; k < NumComponents; k++)
            {
                GetPixelReference(j).SetComponent(k, float_data[i + k]);
            }
        }
    }

    Bitmap(ConstByteView bytes, uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_buffer.SetSize(GetByteSize());

        HYP_CORE_ASSERT(bytes.Size() == GetByteSize(), "Byte view size does not match bitmap size! (%zu != %zu)", bytes.Size(), GetByteSize());

        m_buffer.Write(MathUtil::Min(m_buffer.Size(), bytes.Size()), 0, bytes.Data());
    }

    Bitmap(uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_buffer.SetSize(GetByteSize());
    }

    Bitmap(const Bitmap& other)
        : m_width(other.m_width),
          m_height(other.m_height),
          m_buffer(other.m_buffer)
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
        m_buffer = other.m_buffer;

        return *this;
    }

    Bitmap(Bitmap&& other) noexcept
        : m_width(other.m_width),
          m_height(other.m_height),
          m_buffer(std::move(other.m_buffer))
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
        m_buffer = std::move(other.m_buffer);

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
            * SizeType(NumComponents)
            * sizeof(PixelComponentType);
    }

    // Get reference to the pixel at the given 1-dimensional index
    HYP_FORCE_INLINE PixelReference<PixelComponentType, NumComponents> GetPixelReference(uint32 idx)
    {
        if (HYP_UNLIKELY(m_width * m_height == 0))
        {
            return PixelReference<PixelComponentType, NumComponents>(nullptr);
        }

        const uint32 byteIndex = SizeType(idx % (m_width * m_height)) * SizeType(NumComponents) * sizeof(PixelComponentType);

        return PixelReference<PixelComponentType, NumComponents>(m_buffer.Data() + byteIndex);
    }

    // Get reference to pixel at x,y
    HYP_FORCE_INLINE PixelReference<PixelComponentType, NumComponents> GetPixelReference(uint32 x, uint32 y)
    {
        const uint32 index = (((y) + m_height) % m_height) * m_width
            + ((x + m_width) % m_width);

        return PixelReference<PixelComponentType, NumComponents>(m_buffer.Data() + index * NumComponents * sizeof(PixelComponentType));
    }

    // Get reference to the pixel at the given 1-dimensional index
    HYP_FORCE_INLINE ConstPixelReference<PixelComponentType, NumComponents> GetPixelReference(uint32 idx) const
    {
        return const_cast<Bitmap<NumComponents, PixelComponentType>*>(this)->GetPixelReference(idx);
    }

    HYP_FORCE_INLINE ConstPixelReference<PixelComponentType, NumComponents> GetPixelReference(uint32 x, uint32 y) const
    {
        return const_cast<Bitmap<NumComponents, PixelComponentType>*>(this)->GetPixelReference(x, y);
    }

    HYP_FORCE_INLINE void SetPixel(uint32 x, uint32 y, const Vec4f& rgba)
    {
        GetPixelReference(x, y).SetRGBA(rgba);
    }

    void SetPixels(const ByteBuffer& byteBuffer)
    {
        HYP_CORE_ASSERT(byteBuffer.Size() == GetByteSize(), "Byte buffer size does not match bitmap size! (%zu != %zu)", byteBuffer.Size(), GetByteSize());

        const uint32 numComponents = NumComponents;

        m_buffer = byteBuffer;
    }

    void SetPixels(ByteBuffer&& byteBuffer)
    {
        HYP_CORE_ASSERT(byteBuffer.Size() == GetByteSize(), "Byte buffer size does not match bitmap size! (%zu != %zu)", byteBuffer.Size(), GetByteSize());

        const uint32 numComponents = NumComponents;

        m_buffer = std::move(byteBuffer);
    }

    ByteView ToByteView()
    {
        return m_buffer.ToByteView();
    }

    ConstByteView ToByteView() const
    {
        return m_buffer.ToByteView();
    }

    ByteBuffer GetUnpackedBytes(uint32 bytesPerPixel) const
    {
        ByteBuffer byteBuffer;
        byteBuffer.SetSize((m_width * m_height) * bytesPerPixel);

        ubyte* bytes = byteBuffer.Data();

        if (bytesPerPixel == 1)
        {
            for (uint32 x = 0; x < m_width; x++)
            {
                for (uint32 y = 0; y < m_height; y++)
                {
                    ConstPixelReference<PixelComponentType, NumComponents> pixelReference = GetPixelReference(x, y);
                    Vec4f rgba = pixelReference.GetRGBA();

                    const Color color { rgba };

                    for (uint32 j = 0; j < MathUtil::Min(NumComponents, bytesPerPixel); j++)
                    {
                        bytes[((m_height - y - 1) * m_width + x) * bytesPerPixel + j] = color.bytes[j];
                    }
                }
            }
        }
        else
        {
            for (uint32 x = 0; x < m_width; x++)
            {
                for (uint32 y = 0; y < m_height; y++)
                {
                    ConstPixelReference<PixelComponentType, NumComponents> pixelReference = GetPixelReference(x, y);

                    for (uint32 j = 0; j < MathUtil::Min(NumComponents, bytesPerPixel); j++)
                    {
                        bytes[((m_height - y - 1) * m_width + x) * bytesPerPixel + j] = ubyte(pixelReference.GetComponent(j) * 255.0f);
                    }
                }
            }
        }

        return byteBuffer;
    }

    Array<float> GetUnpackedFloats() const
    {
        Array<float> floats;
        floats.Resize(m_width * m_height * NumComponents);

        for (uint32 x = 0; x < m_width; x++)
        {
            for (uint32 y = 0; y < m_height; y++)
            {
                ConstPixelReference<PixelComponentType, NumComponents> pixelReference = GetPixelReference(x, y);

                for (uint32 j = 0; j < NumComponents; j++)
                {
                    floats[(y * m_width + x) * NumComponents + j] = pixelReference.GetComponent(j);
                }
            }
        }

        return floats;
    }

    /*! \brief Writes the Bitmap's data as a BMP file to the output stream */
    bool Write(ByteWriter* byteWriter) const
    {
        // WriteBitmap uses 3 bytes per pixel
        ByteBuffer unpackedBytes = GetUnpackedBytes(3);

        // BMP stores in BGR format, so swap R and B
        for (SizeType i = 0; i < unpackedBytes.Size(); i += 3)
        {
            Swap(unpackedBytes.Data()[i], unpackedBytes.Data()[i + 2]);
        }

        return WriteBitmap::Write(byteWriter, m_width, m_height, unpackedBytes.Data());
    }

    void FlipVertical()
    {
        for (uint32 x = 0; x < m_width; x++)
        {
            for (uint32 y = 0; y < m_height / 2; y++)
            {
                const Vec4f temp = GetPixelReference(x, m_height - y - 1u).GetRGBA();
                GetPixelReference(x, m_height - y - 1u).SetRGBA(GetPixelReference(x, y).GetRGBA());
                GetPixelReference(x, y).SetRGBA(temp);
            }
        }
    }

    void FlipHorizontal()
    {
        for (uint32 x = 0; x < m_width / 2; x++)
        {
            for (uint32 y = 0; y < m_height; y++)
            {
                const Vec4f temp = GetPixelReference(m_width - x - 1u, y).GetRGBA();
                GetPixelReference(m_width - x - 1u, y).SetRGBA(GetPixelReference(x, y).GetRGBA());
                GetPixelReference(x, y).SetRGBA(temp);
            }
        }
    }

    void FillRectangle(Vec2i p0, Vec2i p1, const Vec4f& color)
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
    void FillTriangle(Vec2i t0, Vec2i t1, Vec2i t2, const Vec4f& color)
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

    void DrawLine(uint32 x0, uint32 y0, uint32 x1, uint32 y1, const Vec4f& color)
    {
        const int dx = MathUtil::Abs(int(x1) - int(x0));
        const int dy = MathUtil::Abs(int(y1) - int(y0));

        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;

        int err = dx - dy;

        while (true)
        {
            GetPixelReference(x0, y0).SetRGBA(color);

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

                const Vec4f c00 = sx0 < other.GetWidth() && sy0 < other.GetHeight() ? other.GetPixelReference(sx0, sy0).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c10 = sx1 < other.GetWidth() && sy0 < other.GetHeight() ? other.GetPixelReference(sx1, sy0).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c01 = sx0 < other.GetWidth() && sy1 < other.GetHeight() ? other.GetPixelReference(sx0, sy1).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c11 = sx1 < other.GetWidth() && sy1 < other.GetHeight() ? other.GetPixelReference(sx1, sy1).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

                const Vec4f c0 = MathUtil::Lerp(c00, c10, tx);
                const Vec4f c1 = MathUtil::Lerp(c01, c11, tx);

                Vec4f resultColor = MathUtil::Lerp(c0, c1, ty);

                GetPixelReference(uint32(dstStartX + i * dstStepX), uint32(dstStartY + j * dstStepY)).SetRGBA(resultColor);
            }
        }
    }

private:
    uint32 m_width;
    uint32 m_height;
    ByteBuffer m_buffer;
};

} // namespace hyperion

#endif
