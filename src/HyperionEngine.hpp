/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/system/AppContext.hpp>
#include <core/Defines.hpp>
#include <Engine.hpp>

namespace hyperion {

HYP_API void InitializeAppContext(RC<AppContext> app_context);
HYP_API void ShutdownApplication();

} // namespace hyperion