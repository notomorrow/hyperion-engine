/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>

#include <rendering/Light.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/memory/Memory.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <type_traits>

using namespace hyperion;

extern "C" {
HYP_EXPORT uint32 Light_GetTypeID()
{
    return TypeID::ForType<Light>().Value();
}

HYP_EXPORT void Light_Create(
    uint32 type,
    Vec3f *position,
    Color *color,
    float intensity,
    float radius,
    ManagedHandle *out_handle
)
{
    *out_handle = CreateManagedHandleFromHandle(CreateObject<Light>(
        LightType(type),
        *position,
        *color,
        intensity,
        radius
    ));
}

HYP_EXPORT void Light_Init(ManagedHandle light_handle)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    InitObject(light);
}

HYP_EXPORT void Light_GetPosition(ManagedHandle light_handle, Vec3f *out_position)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    *out_position = light->GetPosition();
}

HYP_EXPORT void Light_SetPosition(ManagedHandle light_handle, Vec3f *position)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    light->SetPosition(*position);
}

HYP_EXPORT void Light_GetColor(ManagedHandle light_handle, Color *out_color)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    *out_color = light->GetColor();
}

HYP_EXPORT void Light_SetColor(ManagedHandle light_handle, Color *color)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    light->SetColor(*color);
}

HYP_EXPORT float Light_GetIntensity(ManagedHandle light_handle)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return 0.0f;
    }

    return light->GetIntensity();
}

HYP_EXPORT void Light_SetIntensity(ManagedHandle light_handle, float intensity)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    light->SetIntensity(intensity);
}

HYP_EXPORT float Light_GetRadius(ManagedHandle light_handle)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return 0.0f;
    }

    return light->GetRadius();
}

HYP_EXPORT void Light_SetRadius(ManagedHandle light_handle, float radius)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    light->SetRadius(radius);
}

HYP_EXPORT float Light_GetFalloff(ManagedHandle light_handle)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return 0.0f;
    }

    return light->GetFalloff();
}

HYP_EXPORT void Light_SetFalloff(ManagedHandle light_handle, float falloff)
{
    Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

    if (!light) {
        return;
    }

    light->SetFalloff(falloff);
}
} // extern "C"