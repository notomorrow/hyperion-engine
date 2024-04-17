/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Engine.hpp>
#include <system/Application.hpp>
#include <core/Defines.hpp>

namespace hyperion {

HYP_API void InitializeApplication(RC<Application> application);
HYP_API void ShutdownApplication();

} // namespace hyperion