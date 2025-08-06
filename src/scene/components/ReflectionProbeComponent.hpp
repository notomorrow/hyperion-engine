/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/object/Handle.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/Camera.hpp>

#include <core/math/Matrix4.hpp>
#include <core/math/Extent.hpp>
#include <core/math/Vector3.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class EnvProbe;
class ReflectionProbeRenderer;

HYP_STRUCT(Component, Size = 32, Label = "Reflection Probe Component", Description = "Handles cubemap reflection calculations for a single EnvProbe source", Editor = true)
struct ReflectionProbeComponent
{
    HYP_FIELD(Property = "Dimensions", Serialize = true, Editor = true, Label = "Dimensions")
    Vec2u dimensions = Vec2u { 256, 256 };

    HYP_FIELD(Property = "EnvProbe", Serialize = true, Editor = true, Label = "EnvProbe")
    Handle<EnvProbe> envProbe;

    HYP_FIELD(Property = "ReflectionProbeRenderer", Serialize = false, Editor = false)
    RC<ReflectionProbeRenderer> reflectionProbeRenderer;
};

} // namespace hyperion
