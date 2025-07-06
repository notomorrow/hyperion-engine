/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_DEVICE_HPP
#define HYPERION_BACKEND_RENDERER_DEVICE_HPP

#include <rendering/backend/RenderObject.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class DeviceBase : public RenderObject<DeviceBase>
{
public:
    virtual ~DeviceBase() override = default;
};

} // namespace hyperion

#endif