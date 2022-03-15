#include "renderer_semaphore.h"
#include "renderer_device.h"

#include <util.h>

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

    /*std::set<WaitSemaphore> waitee_wait_semaphores;
    waitee_wait_semaphores.insert(waitee.m_wait_semaphores.begin(), waitee.m_wait_semaphores.end());

    std::set<SignalSemaphore> signaler_signal_semaphores;
    signaler_signal_semaphores.insert(signaler.m_signal_semaphores.begin(), signaler.m_signal_semaphores.end());

    const auto waitee_wait_semaphores_before = waitee_wait_semaphores;

    for (auto &signal_semaphore : signaler_signal_semaphores) {
        waitee_wait_semaphores.insert(signal_semaphore.ToWaitSemaphore());
    }

    for (const auto &wait_semaphore : waitee_wait_semaphores_before) {
        signaler_signal_semaphores.insert(wait_semaphore.ToSignalSemaphore());
    }

    signaler.m_signal_semaphores.assign(signaler_signal_semaphores.begin(), signaler_signal_semaphores.end());
    signaler.UpdateViews();

    waitee.m_wait_semaphores.assign(waitee_wait_semaphores.begin(), waitee_wait_semaphores.end());
    waitee.UpdateViews();*/
}

SemaphoreChain::SemaphoreChain(const std::vector<VkPipelineStageFlags> &wait_stage_flags,
    const std::vector<VkPipelineStageFlags> &signal_stage_flags)
{
    m_wait_semaphores.reserve(wait_stage_flags.size());
    m_signal_semaphores.reserve(signal_stage_flags.size());

    for (size_t i = 0; i < wait_stage_flags.size(); i++) {
        auto *ref = new SemaphoreRef;
        ref->count = 0;
        ref->semaphore = new Semaphore(wait_stage_flags[i]);

        refs.insert(ref);

        m_wait_semaphores.emplace_back(ref);
    }

    for (size_t i = 0; i < signal_stage_flags.size(); i++) {
        auto *ref = new SemaphoreRef;
        ref->count = 0;
        ref->semaphore = new Semaphore(signal_stage_flags[i]);

        refs.insert(ref);

        m_signal_semaphores.emplace_back(ref);
    }

    UpdateViews();
}


SemaphoreChain::~SemaphoreChain()
{
}

Result SemaphoreChain::Create(Device *device)
{
    for (size_t i = 0; i < m_signal_semaphores.size(); i++) {
        auto &ref = m_signal_semaphores[i];
        
        HYPERION_BUBBLE_ERRORS(ref.Get()->Create(device));

        m_signal_semaphores_view[i] = ref.GetSemaphore();
    }

    for (size_t i = 0; i < m_wait_semaphores.size(); i++) {
        auto &ref = m_wait_semaphores[i];

        HYPERION_BUBBLE_ERRORS(ref.Get()->Create(device));

        m_wait_semaphores_view[i] = ref.GetSemaphore();
    }

    HYPERION_RETURN_OK;
}

Result SemaphoreChain::Destroy(Device *device)
{
    auto result = Result::OK;

    for (auto &semaphore : m_signal_semaphores) {

        if (!--semaphore.ref->count) {
            HYPERION_PASS_ERRORS(semaphore.ref->semaphore->Destroy(device), result);

            auto it = refs.find(semaphore.ref);
            AssertThrow(it != refs.end());

            AssertThrow((*it)->semaphore != nullptr);
            delete (*it)->semaphore;

            delete *it;
            refs.erase(it);
        }
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