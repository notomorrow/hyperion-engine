/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector3.hpp>
#include <core/math/Transform.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/utilities/FormatFwd.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace hyperion {

struct Triangle;

HYP_STRUCT(Size = 32)
struct HYP_API BoundingBox
{
public:
    BoundingBox();
    BoundingBox(const Vec3f& min, const Vec3f& max);

    BoundingBox(const BoundingBox& other) = default;
    BoundingBox& operator=(const BoundingBox& other) = default;

    BoundingBox(BoundingBox&& other) noexcept = default;
    BoundingBox& operator=(BoundingBox&& other) noexcept = default;

    ~BoundingBox() = default;

    HYP_FORCE_INLINE const Vec3f& GetMin() const
    {
        return min;
    }

    HYP_FORCE_INLINE void SetMin(const Vec3f& min)
    {
        this->min = min;
    }

    HYP_FORCE_INLINE const Vec3f& GetMax() const
    {
        return max;
    }

    HYP_FORCE_INLINE void SetMax(const Vec3f& max)
    {
        this->max = max;
    }

    FixedArray<Vec3f, 8> GetCorners() const;

    HYP_FORCE_INLINE Vec3f GetCenter() const
    {
        return (max + min) * 0.5f;
    }

    void SetCorners(const FixedArray<Vec3f, 8>& corners);

    void SetCenter(const Vec3f& center);

    HYP_FORCE_INLINE Vec3f GetExtent() const
    {
        return max - min;
    }

    void SetExtent(const Vec3f& dimensions);

    float GetRadiusSquared() const;
    float GetRadius() const;

    BoundingBox operator*(float scalar) const;
    BoundingBox& operator*=(float scalar);
    BoundingBox operator/(float scalar) const;
    BoundingBox& operator/=(float scalar);
    BoundingBox operator+(const Vec3f& offset) const;
    BoundingBox& operator+=(const Vec3f& offset);
    BoundingBox operator/(const Vec3f& scale) const;
    BoundingBox& operator/=(const Vec3f& scale);
    BoundingBox operator*(const Vec3f& scale) const;
    BoundingBox& operator*=(const Vec3f& scale);

    HYP_FORCE_INLINE bool operator==(const BoundingBox& other) const
    {
        return min == other.min && max == other.max;
    }

    HYP_FORCE_INLINE bool operator!=(const BoundingBox& other) const
    {
        return !operator==(other);
    }

    BoundingBox& Clear();

    /*! \brief Grow the bounding box by the given delta in each direction. Returns a new BoundingBox and does not modify this. */
    BoundingBox Expand(const Vec3f& delta) const;

    /*! \brief Creates a new BoundingBox that is the union of this and the given point. */
    BoundingBox Union(const Vec3f& vec) const;

    /*! \brief Creates a new BoundingBox that is the union of this and the given BoundingBox. */
    BoundingBox Union(const BoundingBox& other) const;

    /*! \brief Creates a new BoundingBox that is the intersection of this and the given BoundingBox. */
    BoundingBox Intersection(const BoundingBox& other) const;

    // do the AABB's overlap at all?
    bool Overlaps(const BoundingBox& other) const;

    // does this AABB completely contain other?
    bool Contains(const BoundingBox& other) const;

    bool ContainsTriangle(const Triangle& triangle) const;
    bool OverlapsTriangle(const Triangle& triangle) const;

    bool ContainsPoint(const Vec3f& vec) const;

    float Area() const;

    HYP_FORCE_INLINE bool IsFinite() const
    {
        return MathUtil::IsFinite(min) && MathUtil::IsFinite(max);
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    HYP_FORCE_INLINE bool IsZero() const
    {
        return min == Vec3f::Zero() && max == Vec3f::Zero();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(min.GetHashCode());
        hc.Add(max.GetHashCode());

        return hc;
    }

    HYP_NODISCARD HYP_FORCE_INLINE static BoundingBox Empty()
    {
        return BoundingBox(MathUtil::MaxSafeValue<Vec3f>(), MathUtil::MinSafeValue<Vec3f>());
    }

    HYP_NODISCARD HYP_FORCE_INLINE static BoundingBox Zero()
    {
        return BoundingBox(Vec3f::Zero(), Vec3f::Zero());
    }

    HYP_NODISCARD HYP_FORCE_INLINE static BoundingBox Infinity()
    {
        return BoundingBox(-MathUtil::Infinity<Vec3f>(), +MathUtil::Infinity<Vec3f>());
    }

    HYP_FIELD(Property = "Min", Serialize = true, Editor = true)
    Vec3f min;

    HYP_FIELD(Property = "Max", Serialize = true, Editor = true)
    Vec3f max;
};

extern HYP_API BoundingBox operator*(const Matrix4& transform, const BoundingBox& aabb);
extern HYP_API BoundingBox operator*(const Transform& transform, const BoundingBox& aabb);

namespace utilities {

template <class StringType>
struct Formatter<StringType, BoundingBox>
{
    auto operator()(const BoundingBox& boundingBox) const
    {
        return StringType("BoundingBox(min: ")
            + Formatter<StringType, Vec3f> {}(boundingBox.min)
            + StringType(", max: ")
            + Formatter<StringType, Vec3f> {}(boundingBox.max)
            + StringType(")");
    }
};

} // namespace utilities

} // namespace hyperion
