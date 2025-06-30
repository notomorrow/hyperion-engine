/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FRAME_HPP
#define HYPERION_BACKEND_RENDERER_FRAME_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <core/Defines.hpp>

#include <core/containers/HashSet.hpp>

#include <core/functional/Delegate.hpp>

#include <Types.hpp>

// #define HYP_DEBUG_USED_DESCRIPTOR_SETS

namespace hyperion {
class FrameBase : public RenderObject<FrameBase>
{
public:
    virtual ~FrameBase() override = default;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual RendererResult ResetFrameState() = 0;

    void UpdateUsedDescriptorSets();

    HYP_FORCE_INLINE uint32 GetFrameIndex() const
    {
        return m_frameIndex;
    }

    HYP_FORCE_INLINE CmdList& GetCommandList()
    {
        return m_commandList;
    }

    HYP_FORCE_INLINE const CmdList& GetCommandList() const
    {
        return m_commandList;
    }

    void MarkDescriptorSetUsed(DescriptorSetBase* descriptorSet);

    Delegate<void, FrameBase*> OnPresent;
    Delegate<void, FrameBase*> OnFrameEnd;

protected:
    FrameBase(uint32 frameIndex)
        : m_frameIndex(frameIndex)
    {
    }

    uint32 m_frameIndex;
    CmdList m_commandList;
    HashSet<DescriptorSetBase*> m_usedDescriptorSets;
};

} // namespace hyperion

#endif