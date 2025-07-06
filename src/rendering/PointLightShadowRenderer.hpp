/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/resource/Resource.hpp>

#include <rendering/RenderSubsystem.hpp>

#include <rendering/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>

namespace hyperion {

class Light;
class EnvProbe;
class Scene;
class RenderShadowMap;
class RenderLight;

HYP_CLASS(NoScriptBindings)
class HYP_API PointLightShadowRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(PointLightShadowRenderer);

public:
    PointLightShadowRenderer(Name name, const Handle<Scene>& parentScene, const Handle<Light>& light, const Vec2u& extent);

    PointLightShadowRenderer(const PointLightShadowRenderer& other) = delete;
    PointLightShadowRenderer& operator=(const PointLightShadowRenderer& other) = delete;
    virtual ~PointLightShadowRenderer() override;

    virtual void OnRemoved() override;
    virtual void OnUpdate(float delta) override;

private:
    virtual void Init() override;

    Handle<Scene> m_parentScene;
    Handle<Light> m_light;
    Vec2u m_extent;
    BoundingBox m_aabb;
    Handle<EnvProbe> m_envProbe;

    TResourceHandle<RenderShadowMap> m_shadowMap;
};

} // namespace hyperion

