#ifndef HYPERION_RENDERER_SEMAPHORE_H
#define HYPERION_RENDERER_SEMAPHORE_H

#include <csignal>

#include "renderer_result.h"
#include <util/non_owning_ptr.h>

#include <vector>
#include <set>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class Device;
class SemaphoreChain;

class Semaphore {
    friend class SemaphoreChain;
    friend class CommandBuffer;
public:
    Semaphore(VkPipelineStageFlags pipeline_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    Semaphore(const Semaphore &other) = delete;
    Semaphore &operator=(const Semaphore &other) = delete;
    ~Semaphore();

    inline VkSemaphore &GetSemaphore() { return m_semaphore; }
    inline VkSemaphore GetSemaphore() const { return m_semaphore; }
    inline VkPipelineStageFlags GetStageFlags() const { return m_pipeline_stage; }

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    VkSemaphore m_semaphore;
    VkPipelineStageFlags m_pipeline_stage;
};

struct SemaphoreRef {
    Semaphore *semaphore;
    mutable uint32_t count;

    inline bool operator<(const SemaphoreRef &other) const
        { return intptr_t(semaphore) < intptr_t(other.semaphore); }
};

enum class SemaphoreType {
    WAIT,
    SIGNAL
};

template <SemaphoreType Type>
struct SemaphoreRefHolder {
    friend class SemaphoreChain;

    //SemaphoreRefHolder() : ref(nullptr) {}
    SemaphoreRefHolder(SemaphoreRef *ref) : ref(ref) { ++ref->count; }
    SemaphoreRefHolder(SemaphoreRefHolder &&other) : ref(std::move(other.ref)) {}
    SemaphoreRefHolder(const SemaphoreRefHolder &other) : ref(other.ref) { ++ref->count; }

    SemaphoreRefHolder &operator=(const SemaphoreRefHolder &other)
    {
        /* Do not increase counter on self-assignment */
        if (this == &other) {
            return *this;
        }

        ref = other.ref;
        ++ref->count;

        return *this;
    }

    ~SemaphoreRefHolder()
    {
        --ref->count;
    }

    bool operator==(const SemaphoreRefHolder &other) const
    {
        return ref == other.ref;
    }

    inline Semaphore *Get() { return ref->semaphore; }
    inline const Semaphore *Get() const { return ref->semaphore; }
    inline VkSemaphore &GetSemaphore() { return ref->semaphore->GetSemaphore(); }
    inline const VkSemaphore &GetSemaphore() const { return ref->semaphore->GetSemaphore(); }
    inline VkPipelineStageFlags GetStageFlags() const { return ref->semaphore->GetStageFlags(); }

    template <SemaphoreType ToType>
    SemaphoreRefHolder<ToType> ConvertHeldType() const
    {
        return SemaphoreRefHolder<ToType>(ref);
    }

private:
    mutable SemaphoreRef *ref;

};

using WaitSemaphore = SemaphoreRefHolder<SemaphoreType::WAIT>;
using SignalSemaphore = SemaphoreRefHolder<SemaphoreType::SIGNAL>;

struct OwnedSemaphoreHolder {
    std::vector<std::unique_ptr<WaitSemaphore>> wait_semaphores;
    std::vector<std::unique_ptr<SignalSemaphore>> signal_semaphores;
};

struct RefSemaphoreHolder {
    std::vector<non_owning_ptr<const WaitSemaphore>> wait_semaphores;
    std::vector<non_owning_ptr<const SignalSemaphore>> signal_semaphores;
};

class SemaphoreChain {
    friend class CommandBuffer;
    friend class Instance;
public:

    static void Link(SemaphoreChain &signaler, SemaphoreChain &waitee);
    
    SemaphoreChain(const std::vector<VkPipelineStageFlags> &wait_stage_flags,
        const std::vector<VkPipelineStageFlags> &signal_stage_flags);
    SemaphoreChain(const SemaphoreChain &other) = delete;
    SemaphoreChain(SemaphoreChain &&other) = delete;
    SemaphoreChain &operator=(const SemaphoreChain &other) = delete;
    ~SemaphoreChain();

    inline auto &GetWaitSemaphores() { return m_wait_semaphores; }
    inline const auto &GetWaitSemaphores() const { return m_wait_semaphores; }
    inline auto &GetSignalSemaphores() { return m_signal_semaphores; }
    inline const auto &GetSignalSemaphores() const { return m_signal_semaphores; }

    inline bool HasWaitSemaphore(const WaitSemaphore &wait_semaphore) const
    {
        return std::any_of(m_wait_semaphores.begin(), m_wait_semaphores.end(), [&wait_semaphore](const WaitSemaphore &item) {
            return wait_semaphore == item;
        });
    }

    inline bool HasSignalSemaphore(const SignalSemaphore &signal_semaphore) const
    {
        return std::any_of(m_signal_semaphores.begin(), m_signal_semaphores.end(), [&signal_semaphore](const SignalSemaphore &item) {
            return signal_semaphore == item;
        });
    }

    inline SemaphoreChain &WaitsFor(const WaitSemaphore &wait_semaphore)
    {
        if (HasWaitSemaphore(wait_semaphore)) {
            return *this;
        }

        m_wait_semaphores.push_back(wait_semaphore);
        m_wait_semaphores_view.push_back(wait_semaphore.GetSemaphore());
        m_wait_semaphores_stage_view.push_back(wait_semaphore.GetStageFlags());

        ++wait_semaphore.ref->count;

        return *this;
    }

    inline SemaphoreChain &WaitsFor(const SignalSemaphore &signal_semaphore)
    {
        return WaitsFor(signal_semaphore.ConvertHeldType<SemaphoreType::WAIT>());
    }

    inline SemaphoreChain &SignalsTo(const SignalSemaphore &signal_semaphore)
    {
        if (HasSignalSemaphore(signal_semaphore)) {
            return *this;
        }

        m_signal_semaphores.push_back(signal_semaphore);
        m_signal_semaphores_view.push_back(signal_semaphore.GetSemaphore());
        m_signal_semaphores_stage_view.push_back(signal_semaphore.GetStageFlags());

        ++signal_semaphore.ref->count;

        return *this;
    }

    inline SemaphoreChain &SignalsTo(const WaitSemaphore &wait_semaphore)
    {
        return SignalsTo(wait_semaphore.ConvertHeldType<SemaphoreType::SIGNAL>());
    }

    /* Wait on all signals from `other` */
    inline SemaphoreChain &operator<<(const SemaphoreChain &signaler)
    {
        for (auto &signal_semaphore : signaler.GetSignalSemaphores()) {
            WaitsFor(signal_semaphore);
        }

        return *this;
    }

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    using SemaphoreView = std::vector<VkSemaphore>;
    using SemaphoreStageView = std::vector<VkPipelineStageFlags>;

    void UpdateViews();

    static std::set<SemaphoreRef*> refs;

    //OwnedSemaphoreHolder m_owned;
    //RefSemaphoreHolder   m_refs;
    std::vector<SignalSemaphore> m_signal_semaphores;
    std::vector<WaitSemaphore> m_wait_semaphores;

    /* Views, for on the fly linear memory access: */
    SemaphoreView m_signal_semaphores_view;
    SemaphoreView m_wait_semaphores_view;
    SemaphoreStageView m_signal_semaphores_stage_view;
    SemaphoreStageView m_wait_semaphores_stage_view;
};

} // namespace renderer
} // namespace hyperion

#endif