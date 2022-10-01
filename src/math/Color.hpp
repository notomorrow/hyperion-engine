#ifndef HYPERION_COLOR_HPP
#define HYPERION_COLOR_HPP

#include <math/Vector4.hpp>

#include <HashCode.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion {

class alignas(UInt32) Color
{
public:
    static constexpr UInt size = 4;

private:
    friend std::ostream &operator<<(std::ostream &out, const Color &color);

    union {
        UInt32 value;
        UByte bytes[size];
    };

public:
    Color();
    Color(Float r, Float g, Float b, Float a = 1.0f);
    explicit Color(Float rgba);
    Color(const Color &other);
    Color(const Vector4 &vec);

    Float GetRed() const { return Float(bytes[0]) / 255.0f; }
    Color &SetRed(Float red) { bytes[0] = red; return *this; }
    Float GetGreen() const { return Float(bytes[1]) / 255.0f; }
    Color &SetGreen(Float green) { bytes[1] = green; return *this; }
    Float GetBlue() const { return Float(bytes[0]) / 255.0f; }
    Color &SetBlue(Float blue) { bytes[2] = blue; return *this; }
    Float GetAlpha() const { return Float(bytes[0]) / 255.0f; }
    Color &SetAlpha(Float alpha) { bytes[3] = alpha; return *this; }
    
    constexpr Float operator[](UInt index) const
        { return Float(bytes[index]) / 255.0f; }

    Color &operator=(const Color &other);
    Color operator+(const Color &other) const;
    Color &operator+=(const Color &other);
    Color operator-(const Color &other) const;
    Color &operator-=(const Color &other);
    Color operator*(const Color &other) const;
    Color &operator*=(const Color &other);
    Color operator/(const Color &other) const;
    Color &operator/=(const Color &other);
    bool operator==(const Color &other) const;
    bool operator!=(const Color &other) const;

    bool operator<(const Color &other) const
        { return std::tie(bytes[0], bytes[1], bytes[2], bytes[3]) < std::tie(other.bytes[0], other.bytes[1], other.bytes[2], other.bytes[3]); }

    Color &Lerp(const Color &to, Float amt);

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(value);

        return hc;
    }
};

static_assert(sizeof(Color) == sizeof(UInt32), "sizeof(Color) must be equal to sizeof uint32");
static_assert(alignof(Color) == alignof(UInt32), "alignof(Color) must be equal to alignof uint32");

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Color);

#endif
