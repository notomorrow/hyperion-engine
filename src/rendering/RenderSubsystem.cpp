/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/RenderCommand.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

RenderEnvironment* RenderSubsystem::GetParent() const
{
    Threads::AssertOnThread(g_renderThread);

    return m_parent;
}

void RenderSubsystem::SetParent(RenderEnvironment* parent)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    m_parent = parent;
}

void RenderSubsystem::RemoveFromEnvironment()
{
    HYP_SCOPE;

    // struct RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)
    //     : public RenderCommand
    // {
    //     Handle<RenderSubsystem> renderSubsystem;

    //     RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)(Handle<RenderSubsystem>&& renderSubsystem)
    //         : renderSubsystem(renderSubsystem)
    //     {
    //     }

    //     virtual ~RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)() override = default;

    //     virtual RendererResult operator()() override
    //     {
    //         if (renderSubsystem)
    //         {
    //             RenderEnvironment* parent = renderSubsystem->GetParent();

    //             if (parent != nullptr)
    //             {
    //                 parent->RemoveRenderSubsystem(renderSubsystem);
    //             }
    //         }

    //         HYPERION_RETURN_OK;
    //     }
    // };

    // PUSH_RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment, HandleFromThis());
}

} // namespace hyperion