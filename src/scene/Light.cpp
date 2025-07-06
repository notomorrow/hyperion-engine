/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Light.hpp>
#include <scene/Material.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <rendering/RenderProxy.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Light

Light::Light()
    : Light(LT_DIRECTIONAL, Vec3f::Zero(), Color::White(), 1.0f, 1.0f)
{
}

Light::Light(LightType type, const Vec3f& position, const Color& color, float intensity, float radius)
    : m_type(type),
      m_position(position),
      m_color(color),
      m_intensity(intensity),
      m_radius(radius),
      m_falloff(1.0f),
      m_spotAngles(Vec2f::Zero())
{
    m_entityInitInfo.canEverUpdate = false;
    m_entityInitInfo.receivesUpdate = false;
    m_entityInitInfo.bvhDepth = 0; // No BVH for lights
    m_entityInitInfo.initialTags = { EntityTag::LIGHT };
}

Light::Light(LightType type, const Vec3f& position, const Vec3f& normal, const Vec2f& areaSize, const Color& color, float intensity, float radius)
    : m_type(type),
      m_position(position),
      m_normal(normal),
      m_areaSize(areaSize),
      m_color(color),
      m_intensity(intensity),
      m_radius(radius),
      m_falloff(1.0f),
      m_spotAngles(Vec2f::Zero())
{
    m_entityInitInfo.canEverUpdate = false;
    m_entityInitInfo.receivesUpdate = false;
    m_entityInitInfo.bvhDepth = 0; // No BVH for lights
    m_entityInitInfo.initialTags = { EntityTag::LIGHT };
}

Light::~Light()
{
}

void Light::Init()
{
    if (m_material.IsValid())
    {
        InitObject(m_material);
    }

    SetReady(true);
}

void Light::SetPosition(const Vec3f& position)
{
    if (m_position == position)
    {
        return;
    }

    m_position = position;

    SetNeedsRenderProxyUpdate();
}

void Light::SetNormal(const Vec3f& normal)
{
    if (m_normal == normal)
    {
        return;
    }

    m_normal = normal;

    SetNeedsRenderProxyUpdate();
}

void Light::SetAreaSize(const Vec2f& areaSize)
{
    if (m_areaSize == areaSize)
    {
        return;
    }

    m_areaSize = areaSize;

    SetNeedsRenderProxyUpdate();
}

void Light::SetColor(const Color& color)
{
    if (m_color == color)
    {
        return;
    }

    m_color = color;

    SetNeedsRenderProxyUpdate();
}

void Light::SetIntensity(float intensity)
{
    if (m_intensity == intensity)
    {
        return;
    }

    m_intensity = intensity;

    SetNeedsRenderProxyUpdate();
}

void Light::SetRadius(float radius)
{
    if (m_radius == radius)
    {
        return;
    }

    m_radius = radius;

    SetNeedsRenderProxyUpdate();
}

void Light::SetFalloff(float falloff)
{
    if (m_falloff == falloff)
    {
        return;
    }

    m_falloff = falloff;

    SetNeedsRenderProxyUpdate();
}

void Light::SetSpotAngles(const Vec2f& spotAngles)
{
    if (m_spotAngles == spotAngles)
    {
        return;
    }

    m_spotAngles = spotAngles;

    SetNeedsRenderProxyUpdate();
}

void Light::SetMaterial(Handle<Material> material)
{
    if (material == m_material)
    {
        return;
    }

    m_material = std::move(material);

    if (IsInitCalled())
    {
        InitObject(m_material);

        SetNeedsRenderProxyUpdate();
    }
}

Pair<Vec3f, Vec3f> Light::CalculateAreaLightRect() const
{
    Vec3f tangent;
    Vec3f bitangent;
    MathUtil::ComputeOrthonormalBasis(m_normal, tangent, bitangent);

    const float halfWidth = m_areaSize.x * 0.5f;
    const float halfHeight = m_areaSize.y * 0.5f;

    const Vec3f center = m_position;

    const Vec3f p0 = center - tangent * halfWidth - bitangent * halfHeight;
    const Vec3f p1 = center + tangent * halfWidth - bitangent * halfHeight;
    const Vec3f p2 = center + tangent * halfWidth + bitangent * halfHeight;
    const Vec3f p3 = center - tangent * halfWidth + bitangent * halfHeight;

    return { p0, p2 };
}

BoundingBox Light::GetAABB() const
{
    if (m_type == LT_DIRECTIONAL)
    {
        return BoundingBox::Infinity();
    }

    if (m_type == LT_AREA_RECT)
    {
        const Pair<Vec3f, Vec3f> rect = CalculateAreaLightRect();

        return BoundingBox::Empty()
            .Union(rect.first)
            .Union(rect.second)
            .Union(m_position + m_normal * m_radius);
    }

    if (m_type == LT_POINT)
    {
        return BoundingBox(GetBoundingSphere());
    }

    return BoundingBox::Empty();
}

BoundingSphere Light::GetBoundingSphere() const
{
    if (m_type == LT_DIRECTIONAL)
    {
        return BoundingSphere::infinity;
    }

    return BoundingSphere(m_position, m_radius);
}

void Light::UpdateRenderProxy(IRenderProxy* proxy)
{
    RenderProxyLight* proxyCasted = static_cast<RenderProxyLight*>(proxy);
    proxyCasted->light = WeakHandleFromThis();

    proxyCasted->lightMaterial = m_material.ToWeak();

    const BoundingBox aabb = GetAABB();

    LightShaderData& bufferData = proxyCasted->bufferData;
    bufferData.lightId = Id().Value();
    bufferData.lightType = uint32(m_type);
    bufferData.colorPacked = uint32(m_color);
    bufferData.radius = m_radius;
    bufferData.falloff = m_falloff;
    bufferData.areaSize = m_areaSize;
    bufferData.positionIntensity = Vec4f(m_position, m_intensity);
    bufferData.normal = Vec4f(m_normal, 0.0f);
    bufferData.spotAngles = m_spotAngles;
    bufferData.materialIndex = ~0u; // materialIndex gets set in WriteBufferData_Light()
    bufferData.aabbMin = Vec4f(aabb.min, 1.0f);
    bufferData.aabbMax = Vec4f(aabb.max, 1.0f);
    bufferData.shadowMapIndex = ~0u; // @TODO
}

#pragma endregion Light

} // namespace hyperion
