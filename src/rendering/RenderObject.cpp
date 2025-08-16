/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderObject.hpp>
#include <rendering/RenderCommandBuffer.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

#pragma region RenderObjectContainerBase

RenderObjectContainerBase::RenderObjectContainerBase(ANSIStringView renderObjectTypeName)
{
}

#pragma endregion RenderObjectContainerBase

} // namespace hyperion