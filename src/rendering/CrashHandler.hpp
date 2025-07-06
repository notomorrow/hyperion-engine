#pragma once
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */


#include <core/Defines.hpp>

#include <rendering/RenderResult.hpp>

namespace hyperion {

class HYP_API CrashHandler
{
public:
    CrashHandler();

    void Initialize();
    void HandleGPUCrash(RendererResult result);

private:
    bool m_isInitialized;
};

} // namespace hyperion

