/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion {
Features::DynamicFunctions Features::dynFunctions = {};

Features::Features()
    : m_physicalDevice(nullptr),
      m_properties({}),
      m_features({})
{
}

Features::Features(VkPhysicalDevice physicalDevice)
    : Features()
{
    SetPhysicalDevice(physicalDevice);
}

void Features::SetPhysicalDevice(VkPhysicalDevice physicalDevice)
{
    if ((m_physicalDevice = physicalDevice))
    {
        m_features.samplerAnisotropy = VK_TRUE;

        vkGetPhysicalDeviceProperties(physicalDevice, &m_properties);
        vkGetPhysicalDeviceFeatures(physicalDevice, &m_features);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memoryProperties);

        AssertThrow(m_features.samplerAnisotropy);

        auto AddToFeaturesChain = [this](auto nextFeature)
        {
            using T = decltype(nextFeature);

            VkBaseOutStructure* chainTop = m_featuresChain.Empty()
                ? nullptr
                : m_featuresChain.Back().Get();

            m_featuresChain.PushBack(MakeUnique<VkBaseOutStructure>(new T(nextFeature)));

            if (chainTop != nullptr)
            {
                chainTop->pNext = m_featuresChain.Back().Get();
            }

            chainTop = m_featuresChain.Back().Get();
        };

        // features

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        m_bufferDeviceAddressFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = VK_NULL_HANDLE
        };

        m_raytracingPipelineFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &m_bufferDeviceAddressFeatures
        };

        m_accelerationStructureFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &m_raytracingPipelineFeatures
        };

        m_multiviewFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,
            .pNext = &m_accelerationStructureFeatures
        };
#else
        m_multiviewFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,
            .pNext = VK_NULL_HANDLE
        };
#endif

        m_indexingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
            .pNext = &m_multiviewFeatures
        };

        m_features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &m_indexingFeatures
        };

        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &m_features2);

        // properties

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        m_raytracingPipelineProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
            .pNext = VK_NULL_HANDLE
        };

        m_accelerationStructureProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = &m_raytracingPipelineProperties
        };

        m_samplerMinmaxProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,
            .pNext = &m_accelerationStructureProperties
        };
#else
        m_samplerMinmaxProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,
            .pNext = VK_NULL_HANDLE
        };
#endif

        m_indexingProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,
            .pNext = &m_samplerMinmaxProperties
        };

        m_properties2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &m_indexingProperties
        };

        vkGetPhysicalDeviceProperties2(m_physicalDevice, &m_properties2);
    }
}

void Features::LoadDynamicFunctions(Device* device)
{
#define HYP_LOAD_FN(name)                                                               \
    do                                                                                  \
    {                                                                                   \
        auto procAddr = vkGetDeviceProcAddr(device->GetDevice(), #name);                \
        if (procAddr == nullptr)                                                        \
        {                                                                               \
            HYP_LOG(RenderingBackend, Error, "Failed to load dynamic function " #name); \
        }                                                                               \
        dynFunctions.name = reinterpret_cast<PFN_##name>(procAddr);                     \
    }                                                                                   \
    while (0)

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
    HYP_LOAD_FN(vkGetBufferDeviceAddressKHR); // currently only used for RT

    if (IsRaytracingSupported() && !IsRaytracingDisabled())
    {
        DebugLog(LogType::Debug, "Raytracing supported, loading raytracing-specific dynamic functions.\n");

        HYP_LOAD_FN(vkCmdBuildAccelerationStructuresKHR);
        HYP_LOAD_FN(vkBuildAccelerationStructuresKHR);
        HYP_LOAD_FN(vkCreateAccelerationStructureKHR);
        HYP_LOAD_FN(vkDestroyAccelerationStructureKHR);
        HYP_LOAD_FN(vkGetAccelerationStructureBuildSizesKHR);
        HYP_LOAD_FN(vkGetAccelerationStructureDeviceAddressKHR);
        HYP_LOAD_FN(vkCmdTraceRaysKHR);
        HYP_LOAD_FN(vkGetRayTracingShaderGroupHandlesKHR);
        HYP_LOAD_FN(vkCreateRayTracingPipelinesKHR);
    }
#endif

    // HYP_LOAD_FN(vkCmdDebugMarkerBeginEXT);
    // HYP_LOAD_FN(vkCmdDebugMarkerEndEXT);
    // HYP_LOAD_FN(vkCmdDebugMarkerInsertEXT);
    // HYP_LOAD_FN(vkDebugMarkerSetNameEXT);

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    HYP_LOAD_FN(vkGetMoltenVKConfigurationMVK);
    HYP_LOAD_FN(vkSetMoltenVKConfigurationMVK);
#endif

#undef HYP_LOAD_FN
}

void Features::SetDeviceFeatures(Device* device)
{
#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    MVKConfiguration* mvkConfig = nullptr;
    size_t sz = 1;
    dynFunctions.vkGetMoltenVKConfigurationMVK(VK_NULL_HANDLE, mvkConfig, &sz);

    mvkConfig = new MVKConfiguration[sz];

    for (size_t i = 0; i < sz; i++)
    {
#ifdef HYP_DEBUG_MODE
        mvkConfig[i].debugMode = true;
#endif
    }

    dynFunctions.vkSetMoltenVKConfigurationMVK(VK_NULL_HANDLE, mvkConfig, &sz);

    delete[] mvkConfig;
#endif
}

} // namespace hyperion
