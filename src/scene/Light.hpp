/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <scene/Entity.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Camera;
class Material;

HYP_ENUM()
enum LightType : uint32
{
    LT_DIRECTIONAL,
    LT_POINT,
    LT_SPOT,
    LT_AREA_RECT,

    LT_MAX
};

HYP_CLASS()
class HYP_API Light final : public Entity
{
    HYP_OBJECT_BODY(Light);

public:
    Light();

    Light(
        LightType type,
        const Vec3f& position,
        const Color& color,
        float intensity,
        float radius);

    Light(
        LightType type,
        const Vec3f& position,
        const Vec3f& normal,
        const Vec2f& areaSize,
        const Color& color,
        float intensity,
        float radius);

    Light(const Light& other) = delete;
    Light& operator=(const Light& other) = delete;

    Light(Light&& other) noexcept = delete;
    Light& operator=(Light&& other) noexcept = delete;

    ~Light();

    /*! \brief Get the type of the light.
     *
     *  \return The type.
     */
    HYP_METHOD()
    LightType GetLightType() const
    {
        return m_type;
    }

    /*! \brief Get the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \return The position or direction. */
    HYP_METHOD(Property = "Position", Serialize = true, Editor = true)
    const Vec3f& GetPosition() const
    {
        return m_position;
    }

    /*! \brief Set the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \param position The position or direction to set. */
    HYP_METHOD(Property = "Position", Serialize = true, Editor = true)
    void SetPosition(const Vec3f& position);

    /*! \brief Get the normal for the light. This is used only for area lights.
     *
     *  \return The normal. */
    HYP_METHOD(Property = "Normal", Serialize = true, Editor = true)
    const Vec3f& GetNormal() const
    {
        return m_normal;
    }

    /*! \brief Set the normal for the light. This is used only for area lights.
     *
     *  \param normal The normal to set. */
    HYP_METHOD(Property = "Normal", Serialize = true, Editor = true)
    void SetNormal(const Vec3f& normal);

    /*! \brief Get the area size for the light. This is used only for area lights.
     *
     *  \return The area size. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Serialize = true, Editor = true)
    const Vec2f& GetAreaSize() const
    {
        return m_areaSize;
    }

    /*! \brief Set the area size for the light. This is used only for area lights.
     *
     *  \param areaSize The area size to set. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Serialize = true, Editor = true)
    void SetAreaSize(const Vec2f& areaSize);

    /*! \brief Get the color for the light.
     *
     *  \return The color. */
    HYP_METHOD(Property = "Color", Serialize = true, Editor = true)
    const Color& GetColor() const
    {
        return m_color;
    }

    /*! \brief Set the color for the light.
     *
     *  \param color The color to set. */
    HYP_METHOD(Property = "Color", Serialize = true, Editor = true)
    void SetColor(const Color& color);

    /*! \brief Get the intensity for the light. This is used to determine how bright the light is.
     *
     *  \return The intensity. */
    HYP_METHOD(Property = "Intensity", Serialize = true, Editor = true)
    float GetIntensity() const
    {
        return m_intensity;
    }

    /*! \brief Set the intensity for the light. This is used to determine how bright the light is.
     *
     *  \param intensity The intensity to set. */
    HYP_METHOD(Property = "Intensity", Serialize = true, Editor = true)
    void SetIntensity(float intensity);

    /*! \brief Get the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \return The radius. */
    HYP_METHOD(Property = "Radius", Serialize = true, Editor = true)
    float GetRadius() const
    {
        switch (m_type)
        {
        case LT_DIRECTIONAL:
            return INFINITY;
        case LT_POINT:
            return m_radius;
        default:
            return 0.0f;
        }
    }

    /*! \brief Set the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \param radius The radius to set. */
    HYP_METHOD(Property = "Radius", Serialize = true, Editor = true)
    void SetRadius(float radius);

    /*! \brief Get the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \return The falloff. */
    HYP_METHOD(Property = "Falloff", Serialize = true, Editor = true)
    float GetFalloff() const
    {
        return m_falloff;
    }

    /*! \brief Set the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \param falloff The falloff to set. */
    HYP_METHOD(Property = "Falloff", Serialize = true, Editor = true)
    void SetFalloff(float falloff);

    /*! \brief Get the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \return The spotlight angles. */
    HYP_METHOD(Property = "SpotAngles", Serialize = true, Editor = true)
    const Vec2f& GetSpotAngles() const
    {
        return m_spotAngles;
    }

    /*! \brief Set the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \param spotAngles The angles to set for the spotlight. */
    HYP_METHOD(Property = "SpotAngles", Serialize = true, Editor = true)
    void SetSpotAngles(const Vec2f& spotAngles);

    /*! \brief Get the material  for the light. Used for area lights.
     *
     *  \return The material handle associated with the Light. */
    HYP_METHOD(Property = "Material", Serialize = true, Editor = true)
    const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

    /*! \brief Sets the material handle associated with the Light. Used for textured area lights.
     *
     *  \param material The material to set for this Light. */
    HYP_METHOD(Property = "Material", Serialize = true, Editor = true)
    void SetMaterial(Handle<Material> material);

    HYP_METHOD()
    BoundingBox GetAABB() const;

    BoundingSphere GetBoundingSphere() const;

protected:
    void Init() override;
    void UpdateRenderProxy(IRenderProxy* proxy) override;

    // For managed code only - to be removed at some point
    HYP_METHOD()
    void SetLightType(LightType type)
    {
        m_type = type;
    }

    HYP_FIELD(Property = "Type", Serialize = true)
    LightType m_type;
    Vec3f m_position;
    Vec3f m_normal;
    Vec2f m_areaSize;
    Color m_color;
    float m_intensity;
    float m_radius;
    float m_falloff;
    Vec2f m_spotAngles;
    Handle<Material> m_material;

private:
    Pair<Vec3f, Vec3f> CalculateAreaLightRect() const;

    mutable DataMutationState m_mutationState;
};

} // namespace hyperion

