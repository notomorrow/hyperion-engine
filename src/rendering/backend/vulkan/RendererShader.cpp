/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include <core/system/Debug.hpp>

#include <Engine.hpp>

#include <algorithm>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region ShaderPlatformImpl

VkPipelineShaderStageCreateInfo ShaderPlatformImpl<Platform::VULKAN>::CreateShaderStage(const ShaderModule<Platform::VULKAN> &shader_module)
{
    VkPipelineShaderStageCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    create_info.module = shader_module.shader_module;
    create_info.pName = shader_module.entry_point_name.Data();

    switch (shader_module.type) {
    case ShaderModuleType::VERTEX:
        create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case ShaderModuleType::FRAGMENT:
        create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case ShaderModuleType::GEOMETRY:
        create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case ShaderModuleType::COMPUTE:
        create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case ShaderModuleType::TASK:
        create_info.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        break;
    case ShaderModuleType::MESH:
        create_info.stage = VK_SHADER_STAGE_MESH_BIT_NV;
        break;
    case ShaderModuleType::TESS_CONTROL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case ShaderModuleType::TESS_EVAL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case ShaderModuleType::RAY_GEN:
        create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case ShaderModuleType::RAY_INTERSECT:
        create_info.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case ShaderModuleType::RAY_ANY_HIT:
        create_info.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case ShaderModuleType::RAY_CLOSEST_HIT:
        create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case ShaderModuleType::RAY_MISS:
        create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        break;
    default:
        HYP_THROW("Not implemented");
    }

    return create_info;
}

#pragma endregion ShaderPlatformImpl

#pragma region Shader

template <>
Shader<Platform::VULKAN>::Shader(const RC<CompiledShader> &compiled_shader)
    : m_platform_impl { this },
      m_compiled_shader(compiled_shader),
      m_entry_point_name("main")
{
}

template <>
Shader<Platform::VULKAN>::~Shader()
{
}

template <>
bool Shader<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.vk_shader_stages.Size() != 0;
}

template <>
Result Shader<Platform::VULKAN>::AttachSubShader(Device<Platform::VULKAN> *device, ShaderModuleType type, const ShaderObject &shader_object)
{
    const ByteBuffer &spirv = shader_object.bytes;

    VkShaderModuleCreateInfo create_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = spirv.Size();
    create_info.pCode = reinterpret_cast<const uint32 *>(spirv.Data());

    VkShaderModule shader_module;

    HYPERION_VK_CHECK(vkCreateShaderModule(device->GetDevice(), &create_info, nullptr, &shader_module));

    m_shader_modules.PushBack(ShaderModule<Platform::VULKAN>(type, m_entry_point_name, spirv, shader_module));

    std::sort(m_shader_modules.Begin(), m_shader_modules.End());

    HYPERION_RETURN_OK;
}

template <>
Result Shader<Platform::VULKAN>::AttachSubShaders()
{
    if (!m_compiled_shader) {
        return { Result::RENDERER_ERR, "No compiled shader attached" };
    }

    if (!m_compiled_shader->IsValid()) {
        return { Result::RENDERER_ERR, "Attached compiled shader is in invalid state" };
    }

    for (SizeType index = 0; index < m_compiled_shader->modules.Size(); index++) {
        const ByteBuffer &byte_buffer = m_compiled_shader->modules[index];

        if (byte_buffer.Empty()) {
            continue;
        }

        HYPERION_BUBBLE_ERRORS(AttachSubShader(
            g_engine->GetGPUInstance()->GetDevice(),
            ShaderModuleType(index),
            { byte_buffer }
        ));
    }

    HYPERION_RETURN_OK;
}

template <>
Result Shader<Platform::VULKAN>::CreateShaderGroups()
{
    m_shader_groups.Clear();

    for (SizeType i = 0; i < m_shader_modules.Size(); i++) {
        const ShaderModule<Platform::VULKAN> &shader_module = m_shader_modules[i];
        
        switch (shader_module.type) {
        case ShaderModuleType::RAY_MISS: /* fallthrough */
        case ShaderModuleType::RAY_GEN:
            m_shader_groups.PushBack({
                shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                    .generalShader      = uint32(i),
                    .closestHitShader   = VK_SHADER_UNUSED_KHR,
                    .anyHitShader       = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR
                }
            });

            break;
        case ShaderModuleType::RAY_CLOSEST_HIT:
            m_shader_groups.PushBack({
                shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                    .generalShader      = VK_SHADER_UNUSED_KHR,
                    .closestHitShader   = uint32(i),
                    .anyHitShader       = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR
                }
            });

            break;
        default:
            return { Result::RENDERER_ERR, "Unimplemented shader group type" };
        }
    }

    HYPERION_RETURN_OK;
}

template <>
Result Shader<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(AttachSubShaders());

    bool is_raytracing = false;

    for (const ShaderModule<Platform::VULKAN> &shader_module : m_shader_modules) {
        is_raytracing |= shader_module.IsRaytracing();

        m_platform_impl.vk_shader_stages.PushBack(m_platform_impl.CreateShaderStage(shader_module));
    }

    if (is_raytracing) {
        HYPERION_BUBBLE_ERRORS(CreateShaderGroups());
    }

    HYPERION_RETURN_OK;
}

template <>
Result Shader<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (!IsCreated()) {
        HYPERION_RETURN_OK;
    }

    for (const ShaderModule<Platform::VULKAN> &shader_module : m_shader_modules) {
        vkDestroyShaderModule(device->GetDevice(), shader_module.shader_module, nullptr);
    }

    m_shader_modules.Clear();

    HYPERION_RETURN_OK;
}

#pragma endregion Shader

} // namespace platform
} // namespace renderer
} // namespace hyperion