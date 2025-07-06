#pragma once
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */


#include <rendering/RenderObject.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class DeviceBase : public RenderObject<DeviceBase>
{
public:
    virtual ~DeviceBase() override = default;
};

} // namespace hyperion

