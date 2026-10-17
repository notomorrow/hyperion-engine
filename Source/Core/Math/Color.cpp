/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Math/Color.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Constants.hpp>

#ifndef HYP_TOOL
#include <Color.generated.inl>
#endif

namespace Hyperion {

Color Color::operator+(const Color& other) const
{
    return Color(
        GetRed() + other.GetRed(),
        GetGreen() + other.GetGreen(),
        GetBlue() + other.GetBlue(),
        GetAlpha() + other.GetAlpha());
}

Color& Color::operator+=(const Color& other)
{
    SetRed(GetRed() + other.GetRed());
    SetGreen(GetGreen() + other.GetGreen());
    SetBlue(GetBlue() + other.GetBlue());
    SetAlpha(GetAlpha() + other.GetAlpha());

    return *this;
}

Color Color::operator-(const Color& other) const
{
    return Color(
        GetRed() - other.GetRed(),
        GetGreen() - other.GetGreen(),
        GetBlue() - other.GetBlue(),
        GetAlpha() - other.GetAlpha());
}

Color& Color::operator-=(const Color& other)
{
    SetRed(GetRed() - other.GetRed());
    SetGreen(GetGreen() - other.GetGreen());
    SetBlue(GetBlue() - other.GetBlue());
    SetAlpha(GetAlpha() - other.GetAlpha());

    return *this;
}

Color Color::operator*(const Color& other) const
{
    return Color(
        GetRed() * other.GetRed(),
        GetGreen() * other.GetGreen(),
        GetBlue() * other.GetBlue(),
        GetAlpha() * other.GetAlpha());
}

Color& Color::operator*=(const Color& other)
{
    SetRed(GetRed() * other.GetRed());
    SetGreen(GetGreen() * other.GetGreen());
    SetBlue(GetBlue() * other.GetBlue());
    SetAlpha(GetAlpha() * other.GetAlpha());

    return *this;
}

Color Color::operator/(const Color& other) const
{
    return Color(
        GetRed() / MathUtil::Max(other.GetRed(), MathUtil::epsilonF),
        GetGreen() / MathUtil::Max(other.GetGreen(), MathUtil::epsilonF),
        GetBlue() / MathUtil::Max(other.GetBlue(), MathUtil::epsilonF),
        GetAlpha() / MathUtil::Max(other.GetAlpha(), MathUtil::epsilonF));
}

Color& Color::operator/=(const Color& other)
{
    SetRed(GetRed() / MathUtil::Max(other.GetRed(), MathUtil::epsilonF));
    SetGreen(GetGreen() / MathUtil::Max(other.GetGreen(), MathUtil::epsilonF));
    SetBlue(GetBlue() / MathUtil::Max(other.GetBlue(), MathUtil::epsilonF));
    SetAlpha(GetAlpha() / MathUtil::Max(other.GetAlpha(), MathUtil::epsilonF));

    return *this;
}

bool Color::operator==(const Color& other) const
{
    return color32 == other.color32;
}

bool Color::operator!=(const Color& other) const
{
    return color32 != other.color32;
}

Color& Color::Lerp(const Color& to, float amt)
{
    SetRed(MathUtil::Lerp(GetRed(), to.GetRed(), amt));
    SetGreen(MathUtil::Lerp(GetGreen(), to.GetGreen(), amt));
    SetBlue(MathUtil::Lerp(GetBlue(), to.GetBlue(), amt));
    SetAlpha(MathUtil::Lerp(GetAlpha(), to.GetAlpha(), amt));

    return *this;
}

} // namespace Hyperion
