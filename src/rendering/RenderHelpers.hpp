/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

class RenderQueue;
namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod);

} // namespace helpers

class SingleTimeCommands
{
public:
    HYP_API virtual ~SingleTimeCommands() = default;

    void Push(Proc<void(RenderQueue& renderQueue)>&& fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    virtual RendererResult Execute() = 0;

protected:
    Array<Proc<void(RenderQueue& renderQueue)>> m_functions;
};

} // namespace hyperion
