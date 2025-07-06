/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/resource/Resource.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderSubsystem.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

#if 0
class EnvProbe;

class HYP_API ReflectionProbeRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(ReflectionProbeRenderer);

public:
    ReflectionProbeRenderer(Name name, const Handle<EnvProbe>& envProbe);

    ReflectionProbeRenderer(const ReflectionProbeRenderer& other) = delete;
    ReflectionProbeRenderer& operator=(const ReflectionProbeRenderer& other) = delete;
    virtual ~ReflectionProbeRenderer();

private:
    virtual void Init() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(float delta) override;

    Handle<EnvProbe> m_envProbe;
};
#endif

} // namespace hyperion
