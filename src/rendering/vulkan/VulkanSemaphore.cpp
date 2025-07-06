/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/RenderDevice.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanSemaphore::VulkanSemaphore(VkPipelineStageFlags pipelineStage)
    : m_semaphore(nullptr),
      m_pipelineStage(pipelineStage)
{
}

VulkanSemaphore::~VulkanSemaphore()
{
    HYP_GFX_ASSERT(m_semaphore == nullptr, "semaphore should have been destroyed");
}

RendererResult VulkanSemaphore::Create()
{
    VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(GetRenderBackend()->GetDevice()->GetDevice(), &semaphoreInfo, nullptr, &m_semaphore),
        "Failed to create semaphore");

    HYPERION_RETURN_OK;
}

RendererResult VulkanSemaphore::Destroy()
{
    vkDestroySemaphore(GetRenderBackend()->GetDevice()->GetDevice(), m_semaphore, nullptr);
    m_semaphore = nullptr;

    HYPERION_RETURN_OK;
}

std::set<VulkanSemaphoreRef*> VulkanSemaphoreChain::s_refs {};

VulkanSemaphoreChain::VulkanSemaphoreChain(
    const std::vector<VkPipelineStageFlags>& waitStageFlags,
    const std::vector<VkPipelineStageFlags>& signalStageFlags)
{
    m_waitSemaphores.reserve(waitStageFlags.size());
    m_signalSemaphores.reserve(signalStageFlags.size());

    for (const VkPipelineStageFlags waitStageFlag : waitStageFlags)
    {
        auto* ref = new VulkanSemaphoreRef(waitStageFlag);

        s_refs.insert(ref);

        m_waitSemaphores.emplace_back(ref);
    }

    for (const VkPipelineStageFlags signalStageFlag : signalStageFlags)
    {
        auto* ref = new VulkanSemaphoreRef(signalStageFlag);

        s_refs.insert(ref);

        m_signalSemaphores.emplace_back(ref);
    }

    UpdateViews();
}

VulkanSemaphoreChain::~VulkanSemaphoreChain()
{
    HYP_GFX_ASSERT(
        std::all_of(m_signalSemaphores.begin(), m_signalSemaphores.end(), [](const VulkanSignalSemaphore& semaphore)
            {
                return semaphore.m_ref == nullptr;
            }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call");

    HYP_GFX_ASSERT(
        std::all_of(m_waitSemaphores.begin(), m_waitSemaphores.end(), [](const VulkanWaitSemaphore& semaphore)
            {
                return semaphore.m_ref == nullptr;
            }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call");
}

RendererResult VulkanSemaphoreChain::Create()
{
    for (size_t i = 0; i < m_signalSemaphores.size(); i++)
    {
        auto& ref = m_signalSemaphores[i];

        HYPERION_BUBBLE_ERRORS(ref.Get().Create());

        m_signalSemaphoresView[i] = ref.Get().GetVulkanHandle();
    }

    for (size_t i = 0; i < m_waitSemaphores.size(); i++)
    {
        auto& ref = m_waitSemaphores[i];

        HYPERION_BUBBLE_ERRORS(ref.Get().Create());

        m_waitSemaphoresView[i] = ref.Get().GetVulkanHandle();
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanSemaphoreChain::Destroy()
{
    RendererResult result;

    const auto decRef = [this, &result](auto& semaphore)
    {
        auto* ref = semaphore.m_ref;

        if (ref == nullptr)
        {
            return;
        }

        if (!--ref->count)
        {
            HYPERION_PASS_ERRORS(ref->semaphore.Destroy(), result);

            auto it = s_refs.find(ref);
            HYP_GFX_ASSERT(it != s_refs.end());

            delete *it;
            s_refs.erase(it);
        }

        semaphore.m_ref = nullptr;
    };

    for (auto& semaphore : m_signalSemaphores)
    {
        decRef(semaphore);
    }

    for (auto& semaphore : m_waitSemaphores)
    {
        decRef(semaphore);
    }

    return result;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::WaitsFor(const VulkanSignalSemaphore& signalSemaphore)
{
    auto waitSemaphore = signalSemaphore.ConvertHeldType<VulkanSemaphoreType::WAIT>();

    if (HasWaitSemaphore(waitSemaphore))
    {
        return *this;
    }

    m_waitSemaphores.push_back(waitSemaphore);
    m_waitSemaphoresView.push_back(waitSemaphore.Get().GetVulkanHandle());
    m_waitSemaphoresStageView.push_back(waitSemaphore.Get().GetVulkanStageFlags());

    return *this;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::WaitsFor(const VulkanSemaphoreChain& signaler)
{
    for (auto& signalSemaphore : signaler.GetSignalSemaphores())
    {
        WaitsFor(signalSemaphore);
    }

    return *this;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::SignalsTo(const VulkanWaitSemaphore& waitSemaphore)
{
    auto signalSemaphore = waitSemaphore.ConvertHeldType<VulkanSemaphoreType::SIGNAL>();

    if (HasSignalSemaphore(signalSemaphore))
    {
        return *this;
    }

    m_signalSemaphores.push_back(signalSemaphore);
    m_signalSemaphoresView.push_back(signalSemaphore.Get().GetVulkanHandle());
    m_signalSemaphoresStageView.push_back(signalSemaphore.Get().GetVulkanStageFlags());

    return *this;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::SignalsTo(VulkanSemaphoreChain& waitee)
{
    for (auto& signalSemaphore : GetSignalSemaphores())
    {
        waitee.WaitsFor(signalSemaphore);
    }

    return waitee;
}

void VulkanSemaphoreChain::UpdateViews()
{
    m_signalSemaphoresView.resize(m_signalSemaphores.size());
    m_signalSemaphoresStageView.resize(m_signalSemaphores.size());
    m_waitSemaphoresView.resize(m_waitSemaphores.size());
    m_waitSemaphoresStageView.resize(m_waitSemaphores.size());

    for (size_t i = 0; i < m_signalSemaphores.size(); i++)
    {
        const auto& semaphore = m_signalSemaphores[i];

        m_signalSemaphoresView[i] = semaphore.Get().GetVulkanHandle();
        m_signalSemaphoresStageView[i] = semaphore.Get().GetVulkanStageFlags();
    }

    for (size_t i = 0; i < m_waitSemaphores.size(); i++)
    {
        const auto& semaphore = m_waitSemaphores[i];

        m_waitSemaphoresView[i] = semaphore.Get().GetVulkanHandle();
        m_waitSemaphoresStageView[i] = semaphore.Get().GetVulkanStageFlags();
    }
}

} // namespace hyperion