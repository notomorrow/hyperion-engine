#include "renderer_semaphore.h"
#include "renderer_device.h"

#include <util.h>

#include <functional>

namespace hyperion {
namespace renderer {
Semaphore::Semaphore(VkPipelineStageFlags pipeline_stage)
    : m_semaphore(nullptr),
      m_pipeline_stage(pipeline_stage)
{
}

Semaphore::~Semaphore()
{
    AssertThrowMsg(m_semaphore == nullptr, "semaphore should have been destroyed");
}

Result Semaphore::Create(Device *device)
{
    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(device->GetDevice(), &semaphore_info, nullptr, &m_semaphore),
        "Failed to create semaphore"
    );

    HYPERION_RETURN_OK;
}

Result Semaphore::Destroy(Device *device)
{
    vkDestroySemaphore(device->GetDevice(), m_semaphore, nullptr);
    m_semaphore = nullptr;

    HYPERION_RETURN_OK;
}

std::set<SemaphoreRef *> SemaphoreChain::refs{};

void SemaphoreChain::Link(SemaphoreChain &signaler, SemaphoreChain &waitee)
{
    for (auto &signal_semaphore : signaler.GetSignalSemaphores()) {
        waitee.WaitsFor(signal_semaphore);
    }
}

SemaphoreChain::SemaphoreChain(const std::vector<VkPipelineStageFlags> &wait_stage_flags,
    const std::vector<VkPipelineStageFlags> &signal_stage_flags)
{
    m_wait_semaphores.reserve(wait_stage_flags.size());
    m_signal_semaphores.reserve(signal_stage_flags.size());

    for (size_t i = 0; i < wait_stage_flags.size(); i++) {
        auto *ref = new SemaphoreRef(wait_stage_flags[i]);

        refs.insert(ref);

        m_wait_semaphores.emplace_back(ref);
    }

    for (size_t i = 0; i < signal_stage_flags.size(); i++) {
        auto *ref = new SemaphoreRef(signal_stage_flags[i]);

        refs.insert(ref);

        m_signal_semaphores.emplace_back(ref);
    }
    
    UpdateViews();
}

SemaphoreChain::~SemaphoreChain()
{
    AssertThrowMsg(
        std::all_of(m_signal_semaphores.begin(), m_signal_semaphores.end(), [](SignalSemaphore &semaphore) {
            return semaphore.ref == nullptr;
        }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call"
    );

    AssertThrowMsg(
        std::all_of(m_wait_semaphores.begin(), m_wait_semaphores.end(), [](WaitSemaphore &semaphore) {
            return semaphore.ref == nullptr;
        }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call"
    );
}

Result SemaphoreChain::Create(Device *device)
{
    for (size_t i = 0; i < m_signal_semaphores.size(); i++) {
        auto &ref = m_signal_semaphores[i];
        
        HYPERION_BUBBLE_ERRORS(ref.Get().Create(device));

        m_signal_semaphores_view[i] = ref.GetSemaphore();
    }

    for (size_t i = 0; i < m_wait_semaphores.size(); i++) {
        auto &ref = m_wait_semaphores[i];

        HYPERION_BUBBLE_ERRORS(ref.Get().Create(device));

        m_wait_semaphores_view[i] = ref.GetSemaphore();
    }

    HYPERION_RETURN_OK;
}

Result SemaphoreChain::Destroy(Device *device)
{
    auto result = Result::OK;

    const auto dec_ref = [&](auto &semaphore) {
        auto *ref = semaphore.ref;

        if (ref == nullptr) {
            return;
        }

        if (!--ref->count) {
            HYPERION_PASS_ERRORS(ref->semaphore.Destroy(device), result);

            auto it = refs.find(ref);
            AssertThrow(it != refs.end());

            delete *it;
            refs.erase(it);
        }

        semaphore.ref = nullptr;
    };

    for (auto &semaphore : m_signal_semaphores) {
        dec_ref(semaphore);
    }

    for (auto &semaphore : m_wait_semaphores) {
        dec_ref(semaphore);
    }
    
    return result;
}

void SemaphoreChain::UpdateViews()
{
    m_signal_semaphores_view.resize(m_signal_semaphores.size());
    m_signal_semaphores_stage_view.resize(m_signal_semaphores.size());
    m_wait_semaphores_view.resize(m_wait_semaphores.size());
    m_wait_semaphores_stage_view.resize(m_wait_semaphores.size());

    for (size_t i = 0; i < m_signal_semaphores.size(); i++) {
        const auto &semaphore = m_signal_semaphores[i];

        m_signal_semaphores_view[i] = semaphore.GetSemaphore();
        m_signal_semaphores_stage_view[i] = semaphore.GetStageFlags();
    }

    for (size_t i = 0; i < m_wait_semaphores.size(); i++) {
        const auto &semaphore = m_wait_semaphores[i];

        m_wait_semaphores_view[i] = semaphore.GetSemaphore();
        m_wait_semaphores_stage_view[i] = semaphore.GetStageFlags();
    }
}


} // namespace renderer
} // namespace hyperion