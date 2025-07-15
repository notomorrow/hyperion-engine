/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/RenderQueue.hpp>

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

    void MarkDescriptorSetUsed(DescriptorSetBase* descriptorSet);

    Delegate<void, FrameBase*> OnPresent;
    Delegate<void, FrameBase*> OnFrameEnd;

    RenderQueue renderQueue;

protected:
    FrameBase(uint32 frameIndex)
        : m_frameIndex(frameIndex)
    {
    }

    uint32 m_frameIndex;
    HashSet<DescriptorSetBase*> m_usedDescriptorSets;
};

} // namespace hyperion
