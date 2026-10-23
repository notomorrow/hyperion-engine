/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GpuTimerBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/Constants.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

#ifdef HYP_TRACY
namespace tracy {
class VkCtx;
} // namespace tracy
#endif

namespace Hyperion {

class VulkanDevice;
class EngineStatGpuTimer;

class VulkanGpuTimerBackend final : public GpuTimerBackendBase
{
public:
    VulkanGpuTimerBackend();
    ~VulkanGpuTimerBackend() override;

    bool Initialize(DeviceBase* device) override;
    void Shutdown() override;
    
    void OnFrameStart() override;

    bool IsSupported() const override;
    double GetTimestampPeriod() const override;

    void WriteStartTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void WriteStopTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer) override;

    void ResolveFrameResults(uint32 completedFrameIndex) override;

    void CollectProfilerZones(VulkanCommandBuffer* cmd) override;

private:
    double ComputeDeltaMs(uint64 start, uint64 end) const;

    void CreateProfilerContext();
    void BeginProfilerZone(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer);
    void EndProfilerZone();

    uint32 GetOrCreateQuerySlot(EngineStatGpuTimer* timer);

    struct PerFrameState
    {
        VkQueryPool queryPool = VK_NULL_HANDLE;
        bool resultsPending = false;
    };

    VulkanDevice* m_device = nullptr;
    FixedArray<PerFrameState, NumFramesInFlight> m_frames;
    double m_timestampPeriod = 0.0;
    bool m_isSupported = false;
    bool m_isEnabled = false;

#ifdef HYP_TRACY
    tracy::VkCtx* m_profilerContext = nullptr;
#endif
};

} // namespace Hyperion
