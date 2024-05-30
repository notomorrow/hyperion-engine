/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_INSTANCE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_INSTANCE_HPP

#include <vector>

#include <core/Defines.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererQueue.hpp>

#include <system/vma/VmaUsage.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

namespace renderer {
namespace platform {

template <>
class Instance<Platform::VULKAN>
{
    static ExtensionMap GetExtensionMap();

    /* Setup debug mode */
    Result SetupDebug();
    Result SetupDebugMessenger();

public:
    Instance();
    Result Initialize(const AppContext &app_context, bool load_debug_layers = false);

    VkInstance GetInstance() const
        { return m_instance; }

    Swapchain<Platform::VULKAN> *GetSwapchain() const
        { return m_swapchain; }
                                                          
    FrameHandler<Platform::VULKAN> *GetFrameHandler() const
        { return frame_handler; }

    StagingBufferPool &GetStagingBufferPool()
        { return m_staging_buffer_pool; }

    const StagingBufferPool &GetStagingBufferPool() const
        { return m_staging_buffer_pool; }

    VmaAllocator GetAllocator() const
        { return allocator; }

    helpers::SingleTimeCommands GetSingleTimeCommands();

    void SetValidationLayers(Array<const char *> validation_layers);

    Device<Platform::VULKAN> *GetDevice() const
        { return m_device; }

    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();
    
    Result Destroy();

    const char                      *app_name;
    const char                      *engine_name;

    /* Per frame data */
    FrameHandler<Platform::VULKAN>  *frame_handler;

private:
    void CreateSurface();

    VkInstance                      m_instance;
    VkSurfaceKHR                    m_surface;

    VmaAllocator                    allocator = nullptr;

    Device<Platform::VULKAN>        *m_device = nullptr;
    Swapchain<Platform::VULKAN>     *m_swapchain = nullptr;
    
    Array<const char *>             validation_layers;

    StagingBufferPool               m_staging_buffer_pool;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT        debug_messenger;
#endif
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //!RENDERER_INSTANCE

