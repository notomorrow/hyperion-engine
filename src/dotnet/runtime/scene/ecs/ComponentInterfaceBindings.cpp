/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/ComponentInterface.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <asset/serialization/Serialization.hpp>

using namespace hyperion;
using namespace fbom;

extern "C" {

#pragma region ComponentInterface

HYP_EXPORT bool ComponentInterface_GetProperty(ComponentInterfaceBase *component_interface, const char *key, ComponentProperty *out_property)
{
    AssertThrow(out_property != nullptr);

    if (!component_interface) {
        return false;
    }

    if (ComponentProperty *property = component_interface->GetProperty(CreateWeakNameFromDynamicString(key))) {
        *out_property = *property;

        return true;
    }

    return false;
}

#pragma endregion ComponentInterface

#pragma region ComponentProperty

HYP_EXPORT bool ComponentProperty_InvokeGetter(ComponentProperty *property, const void *component, FBOMData *out_data)
{
    AssertThrow(property != nullptr);
    AssertThrow(out_data != nullptr);

    if (!property->IsReadable()) {
        return false;
    }

    property->GetGetter()(component, out_data);

    return true;
}

HYP_EXPORT bool ComponentProperty_InvokeSetter(ComponentProperty *property, void *component, FBOMData *data)
{
    AssertThrow(property != nullptr);
    AssertThrow(data != nullptr);

    if (!property->IsWritable()) {
        return false;
    }

    property->GetSetter()(component, data);

    return true;
}

#pragma endregion ComponentProperty

} // extern "C"
