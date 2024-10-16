/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TRIANGLE_HPP
#define HYPERION_TRIANGLE_HPP

#include <math/Vertex.hpp>
#include <math/BoundingBox.hpp>

#include <core/containers/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {
class HYP_API Triangle
{
public:
    Triangle();
    Triangle(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2);
    Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2);
    Triangle(const Triangle &other);
    ~Triangle() = default;

    HYP_FORCE_INLINE Vertex &operator[](SizeType index)
        { return m_points[index]; }

    HYP_FORCE_INLINE const Vertex &operator[](SizeType index) const
        { return m_points[index]; }

    HYP_FORCE_INLINE Vertex &GetPoint(SizeType index)
        { return m_points[index]; }

    HYP_FORCE_INLINE const Vertex &GetPoint(SizeType index) const
        { return m_points[index]; }

    HYP_FORCE_INLINE void SetPoint(SizeType index, const Vertex &value)
        { m_points[index] = value; }

    HYP_FORCE_INLINE Vec3f GetPosition() const
        { return (m_points[0].GetPosition() + m_points[1].GetPosition() + m_points[2].GetPosition()) / 3.0f; }

    HYP_FORCE_INLINE Vec3f GetNormal() const
        { return (m_points[1].GetPosition() - m_points[0].GetPosition()).Cross(m_points[2].GetPosition() - m_points[0].GetPosition()).Normalize(); }

    Vertex &Closest(const Vec3f &vec);
    const Vertex &Closest(const Vec3f &vec) const;
    // bool IntersectRay(const Ray &ray, RayTestResults &out) const;

    BoundingBox GetBoundingBox() const;

    bool ContainsPoint(const Vec3f &pt) const;

private:
    FixedArray<Vertex, 3>   m_points;
};
} // namespace hyperion

#endif
