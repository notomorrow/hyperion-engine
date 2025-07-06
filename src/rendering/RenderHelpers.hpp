/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Platform.hpp>

#include <Types.hpp>

namespace hyperion {

class CmdList;
namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod);

} // namespace helpers

class SingleTimeCommands
{
public:
    HYP_API virtual ~SingleTimeCommands() = default;

    void Push(Proc<void(CmdList& commandList)>&& fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    virtual RendererResult Execute() = 0;

protected:
    Array<Proc<void(CmdList& commandList)>> m_functions;
};

} // namespace hyperion
