/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SCRIPT_COMPONENT_HPP
#define HYPERION_ECS_SCRIPT_COMPONENT_HPP

#include <core/Handle.hpp>

#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

#include <HashCode.hpp>

namespace hyperion {

using ScriptComponentFlags = uint32;

enum ScriptComponentFlagBits : ScriptComponentFlags
{
    SCF_NONE = 0x0,
    SCF_INIT = 0x1
};

struct ScriptInfo
{
    String  assembly_name;
    String  class_name;
};

struct ScriptComponent
{
    ScriptInfo                  script_info;

    RC<dotnet::Assembly>        assembly;
    UniquePtr<dotnet::Object>   object;

    ScriptComponentFlags        flags = SCF_NONE;
};

} // namespace hyperion

#endif