//
// Created by emd22 on 2022-02-20.
//

#include "renderer_shader.h"

#include "../../system/debug.h"

#include <algorithm>

namespace hyperion {
namespace renderer {
void ShaderProgram::AttachShader(Device *_device, const SpirvObject &spirv) {
    this->device = _device;

    VkShaderModuleCreateInfo create_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    create_info.codeSize = spirv.raw.size();
    create_info.pCode = spirv.VkCode();

    VkShaderModule shader_module;

    AssertThrow(vkCreateShaderModule(device->GetDevice(), &create_info, nullptr, &shader_module) == VK_SUCCESS);

    this->shader_modules.emplace_back(spirv, shader_module);

    std::sort(this->shader_modules.begin(), this->shader_modules.end());
}

VkPipelineShaderStageCreateInfo
ShaderProgram::CreateShaderStage(const ShaderModule &shader_module, const char *entry_point) {
    VkPipelineShaderStageCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

    create_info.module = shader_module.shader_module;
    create_info.pName = entry_point;

    switch (shader_module.spirv.type) {
    case SpirvObject::Type::VERTEX:
        create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case SpirvObject::Type::FRAGMENT:
        create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case SpirvObject::Type::GEOMETRY:
        create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case SpirvObject::Type::COMPUTE:
        create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case SpirvObject::Type::TASK:
        create_info.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        break;
    case SpirvObject::Type::MESH:
        create_info.stage = VK_SHADER_STAGE_MESH_BIT_NV;
        break;
    case SpirvObject::Type::TESS_CONTROL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case SpirvObject::Type::TESS_EVAL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case SpirvObject::Type::RAY_GEN:
        create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case SpirvObject::Type::RAY_INTERSECT:
        create_info.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case SpirvObject::Type::RAY_ANY_HIT:
        create_info.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case SpirvObject::Type::RAY_CLOSEST_HIT:
        create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case SpirvObject::Type::RAY_MISS:
        create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        break;
    default:
        throw std::runtime_error("Shader type " + std::to_string(int(shader_module.spirv.type)) + " is currently unimplemented!");
    }

    return create_info;
}

void ShaderProgram::CreateProgram(const char *entry_point) {
    for (auto &shader_module : this->shader_modules) {
        auto stage = this->CreateShaderStage(shader_module, entry_point);

        this->shader_stages.push_back(stage);
    }
}

void ShaderProgram::Destroy() {
    for (auto &shader_module : this->shader_modules) {
        vkDestroyShaderModule(this->device->GetDevice(), shader_module.shader_module, nullptr);
    }
}

} // namespace renderer
} // namespace hyperion